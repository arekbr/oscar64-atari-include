#ifndef ATARI_GSCREEN_H
#define ATARI_GSCREEN_H

#include "types.h"

// Glyph cache for a rectangular field on a normal-width ANTIC 2 screen.
// The application owns the row-major code buffer and the display list,
// CHBAS, colors and DMA. Use gs_mem as the screen and gs_font as the font.
//
// Pass -dGS_CODES=<buffer> -dGS_W=<columns> -dGS_H=<rows> to the compiler.
// Declare the buffer with at least GS_W * GS_H bytes in the source that
// includes this header. A 256-byte alignment is recommended for the buffer.
// GS_DYN defaults to $80. GS_POOL_LOW allows pool_first below 64.
//
// Define an immutable const byte gs_glyphs[2048] atlas. Scan line r of code
// k is at gs_glyphs[256 * r + k]. The pragma below aligns it to 256 bytes.
// Only codes at or above GS_DYN use the atlas.
//
// Codes below GS_DYN map to fixed font slots. Slot 0 is always blank.
// A $FF mapping displays slot 0 and increments gs_err_code. Dynamic codes
// get a slot from the free stack when first used. A slot goes back on the
// stack after a blit that removes its last reference. With no slot free,
// the cell displays slot 0 and is retried on a later blit.
//
// Keep the module code in writable RAM. Initialization and full blits
// update instructions. Use a single foreground caller and keep calls out
// of NMI/IRQ handlers. The module owns gs_mem, gs_font and its state tables.
// For a list blit, keep its generated code in 256-byte-aligned caller RAM.

#ifndef GS_CODES
#error "gscreen: missing -dGS_CODES=<code buffer symbol>"
#endif
#if !defined(GS_W) || !defined(GS_H)
#error "gscreen: pass -dGS_W=<columns> and -dGS_H=<rows>"
#endif
#if GS_W < 1 || GS_W > 40 || GS_H < 1 || GS_H > 24
#error "gscreen: GS_W 1..40, GS_H 1..24"
#endif
#ifndef GS_DYN
#define GS_DYN		0x80
#endif
#if GS_DYN < 1 || GS_DYN > 255
#error "gscreen: GS_DYN 1..255"
#endif

#define GS_CELLS	(GS_W * GS_H)
#define GS_PAGES	((GS_CELLS + 255) / 256)
#define GS_LAST		(GS_CELLS - 256 * (GS_PAGES - 1))	// Cells on the final scan page.

// gs_init reads the list description and encodes its addresses into the
// generated blit. The blit then reads the live position, direction and
// count variables, the tail tables and the code buffer. Keep those objects
// alive. Declare the live variables volatile or name them with compiler
// flags -dGS_REF0=<symbol> through -dGS_REF7=<symbol> so the compiler sees
// their use. The code buffer is covered automatically.
struct GSActor
{
	char * volatile * pos;			// Address of a pointer to a cell in the code buffer.
	const volatile char * dir;		// Tail table index, or null for an actor with no tail.
};

// A list blit visits fixed cells in order, then fixed actors in order,
// then the first min(*count, ncnt) counted actors in reverse order.
// Each actor visits its position, then its tail if present. Positions and
// tail positions outside the code buffer are skipped. Duplicate visits
// are allowed. A direction index must be less than tlen.
struct GSList
{
	const word * cells;				// Fixed cell indices in the code buffer.
	byte ncells;
	const struct GSActor * act;		// nfix fixed actors followed by ncnt counted actors.
	byte nfix, ncnt;
	const volatile char * count;	// Live number of counted actors, required if ncnt is nonzero.
	const signed char * tlo, * thi;	// Low and high bytes of the tail offset for each direction.
	byte tlen;						// Entries in each tail table.
};

// Byte counts for the generated list code.
#if GS_PAGES == 1
#define GS_LCHK_B	(GS_LAST < 256 ? 17 : 13)
#else
#define GS_LCHK_B	(16 + 18 * (GS_PAGES - 2) + (GS_LAST < 256 ? 19 : 15))
#endif
#if GS_DYN == 0x80
#define GS_LCELL_B	34
#else
#define GS_LCELL_B	40
#endif
#define GS_LACT_B	(16 + GS_LCHK_B)
#define GS_LTAIL_B	(15 + GS_LCHK_B)
#define GS_LIST_MAXCNT	32
// Code capacity for nc fixed cells, na total actors and nd counted actors.
// It reserves tail code for every actor. gs_list_need gives the exact size.
#define GS_LIST_SIZE(nc, na, nd)	((nc) * GS_LCELL_B + (na) * (GS_LACT_B + GS_LTAIL_B) + ((nd) ? 13 + 7 * ((nd) - 1) : 0) + 3)

// gs_init reads the descriptor. Call it again to apply descriptor changes.
struct GScreen
{
	byte dyn;						// GS_DYN.
	byte x0, y0;					// Field origin in the 40 by 24 screen.
	const byte * slot_of;			// GS_DYN entries, each below pool_first or $FF.
	const byte * fixed;				// 8 * pool_first bytes, eight per fixed slot.
	byte pool_first;				// 64 through 127, or 1 through 127 with GS_POOL_LOW.
	byte blank;						// Fixed code mapped to slot 0.
	const struct GSList * list;		// Optional description for gs_blit_list.
	byte * code;					// Writable list-code buffer aligned to 256 bytes.
	word code_size;					// Buffer capacity in bytes.
};

// Status from gs_init. Warnings leave the full blit working. An error
// disables both entries when the module recognizes their code layout.
// An unrecognized layout leaves the existing entries alone.
#define GS_OK			0
#define GS_E_NULL		1		// Missing descriptor, fixed map or fixed glyphs.
#define GS_E_DYN		2		// The descriptor's threshold differs from GS_DYN.
#define GS_E_GEOM		3		// The field extends beyond 40 columns or 24 rows.
#define GS_E_POOL		4		// pool_first is outside the supported range.
#define GS_E_BLANK		5		// blank is a dynamic code or does not map to slot 0.
#define GS_E_SLOT		6		// A fixed code maps into or beyond the dynamic pool.
#define GS_E_CORE		7		// The blit entries or core have an unexpected code layout.
#define GS_E_LCELL		8		// Missing fixed cell indices or an out-of-range index.
#define GS_E_LACT		9		// Invalid actor count, pointer or tail data.
#define GS_E_LGEN		10		// Code generation failed and may have written the code buffer.
#define GS_W_NOLIST		0x80	// No list was supplied.
#define GS_W_LISTBUF	0x81	// The list-code buffer is missing or too small.
#define GS_W_LISTALIGN	0x82	// The list-code buffer is misaligned.

byte gs_init(const struct GScreen * s);

// Exact list-code size after validation, even for a buffer warning.
// Zero after successful initialization without a list.
extern volatile word gs_list_need;

// Visit all field cells in index order. Only changed cells are written.
// Before initialization, the entry has no display effect.
void gs_blit(void);

// Visit only the supplied list. Other cells wait for a full blit. The
// first blit is full after initialization. With a warning, all are full.
void gs_blit_list(void);

extern volatile byte gs_mem[960];		// 40 by 24 ANTIC 2 screen, 1 KB aligned.
extern volatile byte gs_font[1024];		// 128 glyphs, eight bytes each, 1 KB aligned.
extern volatile byte gs_err_pool;		// Failed slot allocations, saturates at 255.
extern volatile byte gs_err_code;		// Unmapped fixed codes, saturates at 255.
extern volatile byte gs_pool_peak;		// Largest pool occupancy before end-of-blit releases.
#ifdef GS_AUDIT
// Optional test counters reset by successful initialization.
extern volatile byte gs_audit;			// Cells using a slot when its glyph is replaced.
extern volatile byte gs_rel_mid;		// Slot releases detected during a blit.
extern volatile byte gs_aud_nl, gs_aud_nh;	// Low and high bytes of the glyph check count.
#endif

extern const byte gs_glyphs[];
#pragma align(gs_glyphs, 256)

// Read-only state. gs_prev, gs_dlo and gs_dhi are indexed by field cell.
// gs_slot maps code to slot. gs_owner maps a pool slot to its code, or
// zero when free. gs_ref[k] and gs_ref[256 + k] hold the low and high
// reference count bytes for a dynamic code. gs_free is a stack of slots.
// gs_pend contains codes that lost their last reference during a blit.
extern volatile byte gs_prev[GS_CELLS], gs_dlo[GS_CELLS], gs_dhi[GS_CELLS];
extern volatile byte gs_slot[256], gs_owner[128], gs_ref[512], gs_free[128], gs_pend[256];
extern volatile byte gs_free_n, gs_pend_n, gs_pend_full;

#pragma compile("gscreen.c")

#endif
