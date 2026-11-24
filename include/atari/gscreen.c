#include "gscreen.h"

// Page-aligned tables back the diff blit and the generated list blit.
// gs_init validates their entry points before patching the executable code.

static_assert(sizeof(gs_glyphs) == 2048, "gscreen: define const byte gs_glyphs[2048] in the application");
static_assert(sizeof(GS_CODES) >= GS_CELLS, "gscreen: declare the -dGS_CODES buffer with at least GS_W * GS_H bytes");

volatile byte gs_mem[960];
#pragma align(gs_mem, 1024)
volatile byte gs_font[1024];
#pragma align(gs_font, 1024)

volatile byte gs_prev[GS_CELLS];		// Previous codes, arranged in 256-byte scan pages.
#pragma align(gs_prev, 256)
// Address before indexing by Y. For cell i, subtract i & 255 from its screen address.
volatile byte gs_dlo[GS_CELLS];
#pragma align(gs_dlo, 256)
volatile byte gs_dhi[GS_CELLS];
#pragma align(gs_dhi, 256)
volatile byte gs_slot[256];				// Code to slot, or $FF when no slot is assigned.
#pragma align(gs_slot, 256)
volatile byte gs_owner[128];			// Dynamic slot to code, or zero when free.
#pragma align(gs_owner, 128)
// Reference counts for dynamic codes. gs_ref[k] is low and gs_ref[256 + k] high.
volatile byte gs_ref[512];
#pragma align(gs_ref, 256)
#define REFL	gs_ref
#define REFH	gs_ref + 256
volatile byte gs_pend[256];				// Codes that lost their last cell in this blit.
#pragma align(gs_pend, 256)
volatile byte gs_free[128];				// Stack of free dynamic slots.
#pragma align(gs_free, 128)
volatile byte gs_pend_n, gs_pend_full, gs_free_n;
volatile byte gs_err_pool, gs_err_code, gs_pool_peak;
static volatile byte gs_blankv;			// The blank code used when allocation fails.

// Low byte of the font offset for a slot.
#define M8(s)	(byte)((s) * 8)
#define M8x8(s)	M8(s), M8(s + 1), M8(s + 2), M8(s + 3), M8(s + 4), M8(s + 5), M8(s + 6), M8(s + 7)
#define M8x32(s)	M8x8(s), M8x8(s + 8), M8x8(s + 16), M8x8(s + 24)
static const byte gs_mul8[128] = { M8x32(0), M8x32(32), M8x32(64), M8x32(96) };
#pragma align(gs_mul8, 128)

// Zero-page workspace used by the assembly core and generated list.
static __zeropage byte * gs_zp;			// Indexed destination in gs_mem.
static __zeropage byte gs_zy, gs_zx;	// Saved Y and X for cell writes and slot releases.
static __zeropage byte gs_zlo, gs_zhi;	// Low and high bytes of a generated actor offset.

#ifdef GS_POOL_LOW
#define GS_SCAN0	1
#else
#define GS_SCAN0	64
#endif

// Branch to L if the old code is below the dynamic threshold.
#if GS_DYN == 0x80
#define GS_OLD_LO(L)	bpl L
#else
#define GS_OLD_LO(L)	cmp #GS_DYN; bcc L
#endif

// Copy the glyph for code X at offset Y within the selected font quadrant.
#define GLYPH(base)	lda gs_glyphs, x; sta base, y; lda gs_glyphs + 256, x; sta base + 1, y; lda gs_glyphs + 512, x; sta base + 2, y; \
	lda gs_glyphs + 768, x; sta base + 3, y; lda gs_glyphs + 1024, x; sta base + 4, y; lda gs_glyphs + 1280, x; sta base + 5, y; \
	lda gs_glyphs + 1536, x; sta base + 6, y; lda gs_glyphs + 1792, x; sta base + 7, y

// Select the font quadrant for slot Y, copy the glyph and branch to C.
// Pass the final labels in because nested macro concatenation can lose them.
#ifdef GS_POOL_LOW
#define GQ(GH, GL, G1, C)	lda gs_mul8, y; cpy #64; bcc GL; cpy #96; tay; bcs GH; \
	GLYPH(gs_font + 512); ldy gs_zy; lda gs_slot, x; jmp C; \
	GH: GLYPH(gs_font + 768); ldy gs_zy; lda gs_slot, x; jmp C; \
	GL: cpy #32; tay; bcs G1; \
	GLYPH(gs_font); ldy gs_zy; lda gs_slot, x; jmp C; \
	G1: GLYPH(gs_font + 256); ldy gs_zy; lda gs_slot, x; jmp C
#else
#define GQ(GH, GL, G1, C)	cpy #96; lda gs_mul8, y; tay; bcs GH; \
	GLYPH(gs_font + 512); ldy gs_zy; lda gs_slot, x; jmp C; \
	GH: GLYPH(gs_font + 768); ldy gs_zy; lda gs_slot, x; jmp C
#endif

// Changed cell, with the new code in A and the index within the page in Y.
// PV is the copy, DH and DL are the address tables, and CONT follows the write.
// Keep assembly comments outside the macro bodies.
#define CELL(S, PV, DH, DL, CONT) \
	.ch##S: inc REFH, x; jmp .w##S; \
	.ok##S: lda REFL, x; cmp #$ff; bne .ox##S; dec REFH, x; \
	.ox##S: ldx gs_zx; jmp .r##S; \
	.f##S: tax; lda PV, y; GS_OLD_LO(.r##S); \
	.o##S: stx gs_zx; tax; dec REFL, x; beq .oz##S; bmi .ok##S; \
	.ob##S: \
	.oe##S: ldx gs_zx; \
	.r##S: txa; sta PV, y; lda gs_slot, x; bmi .a##S; cpx #GS_DYN; bcc .w##S; \
	.c##S: inc REFL, x; beq .ch##S; \
	.w##S: ldx DH, y; stx gs_zp + 1; ldx DL, y; stx gs_zp; sta (gs_zp), y; \
	CONT; \
	.oz##S: lda REFH, x; bne .ox##S; txa; ldx gs_pend_n; inx; beq .op##S; stx gs_pend_n; sta gs_pend - 1, x; \
	.ow##S: ldx gs_zx; jmp .r##S; \
	.op##S: inx; stx gs_pend_full; bne .ow##S; \
	.ec##S: lda gs_err_code; cmp #$ff; beq .ez##S; inc gs_err_code; \
	.ez##S: lda #0; jmp .w##S; \
	.ep##S: lda gs_err_pool; cmp #$ff; beq .pz##S; inc gs_err_pool; \
	.pz##S: lda gs_blankv; sta PV, y; lda #0; jmp .w##S; \
	.a##S: cpx #GS_DYN; bcc .ec##S; lda gs_free_n; beq .ep##S; sty gs_zy; tay; dey; sty gs_free_n; lda gs_free, y; sta gs_slot, x; tay; \
	txa; sta gs_owner, y; GQ(.gh##S, .gl##S, .g1##S, .c##S)

// Finish the blit by recording peak occupancy and releasing slots whose
// reference counts reached zero. A code may appear twice in gs_pend, so
// check ownership before release. A full pending list triggers a pool scan.
// gs_init patches in the pool capacity. The common exit stays next to .rl.
#define FIN \
	.fin: lda #0; sec; sbc gs_free_n; cmp gs_pool_peak; bcc .pk; sta gs_pool_peak; \
	.pk: lda gs_pend_full; bne .rf; ldx gs_pend_n; beq .rend; stx gs_zx; ldx #0; \
	.rl: ldy gs_pend, x; lda REFL, y; ora REFH, y; bne .rn; lda gs_slot, y; bmi .rn; sta gs_zy; lda #$ff; sta gs_slot, y; \
	ldy gs_zy; lda #0; \
	sta gs_owner, y; tya; ldy gs_free_n; sta gs_free, y; inc gs_free_n; \
	.rn: inx; cpx gs_zx; bne .rl; beq .rend; \
	.rend: lda #0; sta gs_pend_n; sta gs_pend_full; rts; \
	.rf: ldx #GS_SCAN0; \
	.sl: ldy gs_owner, x; beq .sn; lda REFL, y; ora REFH, y; bne .sn; lda gs_slot, y; bmi .sn; stx gs_zx; tax; \
	lda #$ff; sta gs_slot, y; \
	lda #0; sta gs_owner, x; txa; ldx gs_free_n; sta gs_free, x; inc gs_free_n; ldx gs_zx; \
	.sn: inx; bpl .sl; bmi .rend

#ifdef GS_AUDIT
// The test build inserts checks before glyph writes and around the release
// pass. Keep these copies of GQ and FIN in step with the main macros.
// The hooks preserve the registers used by the core. They detect overwrite
// of a referenced slot and returns to the free stack before the release pass.
// Initialization resets the counters and establishes the free-stack baseline.
volatile byte gs_audit, gs_rel_mid;
volatile byte gs_aud_nl, gs_aud_nh;	// Low and high bytes of the glyph check count.
static volatile byte gs_aud_fn, gs_aud_s, gs_aud_x, gs_aud_a;

// Scan a full page or a partial final page.
#define GS_AUDF(PG)	ldy #0; \
	.p##PG: lda gs_dlo + 256 * PG, y; sta gs_zp; lda gs_dhi + 256 * PG, y; sta gs_zp + 1; lda (gs_zp), y; cmp gs_aud_s; bne .n##PG; \
	lda gs_audit; cmp #$ff; beq .n##PG; inc gs_audit; \
	.n##PG: iny; bne .p##PG
#define GS_AUDL(PG, N)	ldy #0; \
	.p##PG: lda gs_dlo + 256 * PG, y; sta gs_zp; lda gs_dhi + 256 * PG, y; sta gs_zp + 1; lda (gs_zp), y; cmp gs_aud_s; bne .n##PG; \
	lda gs_audit; cmp #$ff; beq .n##PG; inc gs_audit; \
	.n##PG: iny; cpy #N; bne .p##PG

// Before writing a glyph, preserve slot Y and code X.
__noinline void gs_aud_g(void)
{
	__asm volatile
	{
		stx gs_aud_x
		sty gs_aud_s
		inc gs_aud_nl
		bne .g0
		inc gs_aud_nh
	.g0:	ldx gs_free_n
		inx
		cpx gs_aud_fn
		beq .g1
		lda gs_rel_mid
		cmp #$ff
		beq .g1
		inc gs_rel_mid
	.g1:	lda gs_free_n
		sta gs_aud_fn
#if GS_PAGES > 1
		GS_AUDF(0)
#endif
#if GS_PAGES > 2
		GS_AUDF(1)
#endif
#if GS_PAGES > 3
		GS_AUDF(2)
#endif
#if GS_PAGES == 1
#if GS_LAST < 256
		GS_AUDL(0, GS_LAST)
#else
		GS_AUDF(0)
#endif
#endif
#if GS_PAGES == 2
#if GS_LAST < 256
		GS_AUDL(1, GS_LAST)
#else
		GS_AUDF(1)
#endif
#endif
#if GS_PAGES == 3
#if GS_LAST < 256
		GS_AUDL(2, GS_LAST)
#else
		GS_AUDF(2)
#endif
#endif
#if GS_PAGES == 4
#if GS_LAST < 256
		GS_AUDL(3, GS_LAST)
#else
		GS_AUDF(3)
#endif
#endif
		ldx gs_aud_x
		ldy gs_aud_s
	}
}

// Entry to the release pass, with pool capacity in A.
__noinline void gs_aud_f(void)
{
	__asm volatile
	{
		sta gs_aud_a
		lda gs_free_n
		cmp gs_aud_fn
		beq .f1
		lda gs_rel_mid
		cmp #$ff
		beq .f1
		inc gs_rel_mid
	.f1:	lda gs_aud_a
	}
}

// Record the new free-stack baseline at the end of the release pass.
__noinline void gs_aud_e(void)
{
	__asm volatile
	{
		lda gs_free_n
		sta gs_aud_fn
	}
}

#undef GQ
#ifdef GS_POOL_LOW
#define GQ(GH, GL, G1, C)	jsr gs_aud_g; lda gs_mul8, y; cpy #64; bcc GL; cpy #96; tay; bcs GH; \
	GLYPH(gs_font + 512); ldy gs_zy; lda gs_slot, x; jmp C; \
	GH: GLYPH(gs_font + 768); ldy gs_zy; lda gs_slot, x; jmp C; \
	GL: cpy #32; tay; bcs G1; \
	GLYPH(gs_font); ldy gs_zy; lda gs_slot, x; jmp C; \
	G1: GLYPH(gs_font + 256); ldy gs_zy; lda gs_slot, x; jmp C
#else
#define GQ(GH, GL, G1, C)	jsr gs_aud_g; cpy #96; lda gs_mul8, y; tay; bcs GH; \
	GLYPH(gs_font + 512); ldy gs_zy; lda gs_slot, x; jmp C; \
	GH: GLYPH(gs_font + 768); ldy gs_zy; lda gs_slot, x; jmp C
#endif
#undef FIN
#define FIN \
	.fin: lda #0; jsr gs_aud_f; sec; sbc gs_free_n; cmp gs_pool_peak; bcc .pk; sta gs_pool_peak; \
	.pk: lda gs_pend_full; bne .rf; ldx gs_pend_n; beq .rend; stx gs_zx; ldx #0; \
	.rl: ldy gs_pend, x; lda REFL, y; ora REFH, y; bne .rn; lda gs_slot, y; bmi .rn; sta gs_zy; lda #$ff; sta gs_slot, y; \
	ldy gs_zy; lda #0; \
	sta gs_owner, y; tya; ldy gs_free_n; sta gs_free, y; inc gs_free_n; \
	.rn: inx; cpx gs_zx; bne .rl; beq .rend; \
	.rend: jsr gs_aud_e; lda #0; sta gs_pend_n; sta gs_pend_full; rts; \
	.rf: ldx #GS_SCAN0; \
	.sl: ldy gs_owner, x; beq .sn; lda REFL, y; ora REFH, y; bne .sn; lda gs_slot, y; bmi .sn; stx gs_zx; tax; \
	lda #$ff; sta gs_slot, y; \
	lda #0; sta gs_owner, x; txa; ldx gs_free_n; sta gs_free, x; inc gs_free_n; ldx gs_zx; \
	.sn: inx; bpl .sl; bmi .rend
#endif

// A full page scans until Y wraps. A partial page stops at N.
#define SCANF(LP, FP, PG)	LP: lda GS_CODES + 256 * PG, y; cmp gs_prev + 256 * PG, y; bne FP; iny; bne LP
#define SCANP(LP, FP, PG, N)	LP: cpy #N; beq .done; lda GS_CODES + 256 * PG, y; cmp gs_prev + 256 * PG, y; bne FP; iny; bne LP; \
	.done: jmp .fin

// Static core with a jump table for each page, the release pass and .arm.
// The page handlers also serve as subroutines for the generated list.
// gs_init checks the jump table and patches the release and list operands.
__noinline void gs_core(void)
{
	__asm volatile
	{
		jmp .fL0
#if GS_PAGES > 1
		jmp .fL1
#endif
#if GS_PAGES > 2
		jmp .fL2
#endif
#if GS_PAGES > 3
		jmp .fL3
#endif
		jmp .fin
		jmp .arm

		CELL(L0, gs_prev, gs_dhi, gs_dlo, rts)
#if GS_PAGES > 1
		CELL(L1, gs_prev + 256, gs_dhi + 256, gs_dlo + 256, rts)
#endif
#if GS_PAGES > 2
		CELL(L2, gs_prev + 512, gs_dhi + 512, gs_dlo + 512, rts)
#endif
#if GS_PAGES > 3
		CELL(L3, gs_prev + 768, gs_dhi + 768, gs_dlo + 768, rts)
#endif

	.full:	ldy #0
#if GS_PAGES == 1
#if GS_LAST < 256
		SCANP(.l0, .f0, 0, GS_LAST)
		CELL(0, gs_prev, gs_dhi, gs_dlo, iny; jmp .l0)
#else
		SCANF(.l0, .f0, 0); jmp .fin
		CELL(0, gs_prev, gs_dhi, gs_dlo, iny; bne .l0; jmp .fin)
#endif
#else
		SCANF(.l0, .f0, 0); jmp .l1
		CELL(0, gs_prev, gs_dhi, gs_dlo, iny; bne .l0; jmp .l1)
#if GS_PAGES == 2
#if GS_LAST < 256
		SCANP(.l1, .f1, 1, GS_LAST)
		CELL(1, gs_prev + 256, gs_dhi + 256, gs_dlo + 256, iny; jmp .l1)
#else
		SCANF(.l1, .f1, 1); jmp .fin
		CELL(1, gs_prev + 256, gs_dhi + 256, gs_dlo + 256, iny; bne .l1; jmp .fin)
#endif
#else
		SCANF(.l1, .f1, 1); jmp .l2
		CELL(1, gs_prev + 256, gs_dhi + 256, gs_dlo + 256, iny; bne .l1; jmp .l2)
#if GS_PAGES == 3
#if GS_LAST < 256
		SCANP(.l2, .f2, 2, GS_LAST)
		CELL(2, gs_prev + 512, gs_dhi + 512, gs_dlo + 512, iny; jmp .l2)
#else
		SCANF(.l2, .f2, 2); jmp .fin
		CELL(2, gs_prev + 512, gs_dhi + 512, gs_dlo + 512, iny; bne .l2; jmp .fin)
#endif
#else
		SCANF(.l2, .f2, 2); jmp .l3
		CELL(2, gs_prev + 512, gs_dhi + 512, gs_dlo + 512, iny; bne .l2; jmp .l3)
#if GS_LAST < 256
		SCANP(.l3, .f3, 3, GS_LAST)
		CELL(3, gs_prev + 768, gs_dhi + 768, gs_dlo + 768, iny; jmp .l3)
#else
		SCANF(.l3, .f3, 3); jmp .fin
		CELL(3, gs_prev + 768, gs_dhi + 768, gs_dlo + 768, iny; bne .l3; jmp .fin)
#endif
#endif
#endif
#endif

		FIN

	.arm:	lda #0
		sta gs_blit_list + 1
		lda #0
		sta gs_blit_list + 2
		jmp .full
		rts					// Keep a final RTS so the assembler preserves the preceding jump.
	}
}
#pragma align(gs_core, 256)

// Start with BIT so the compiler keeps calls at the entry address. A JMP
// here would let it shortcut calls directly to the uninitialized target.
// gs_init writes the target before switching to JMP. The fallback at +3
// returns without drawing while the entry is inactive. Its loads tell
// the compiler which memory the generated code reads, and TAX/TAY tell
// it that X and Y may change. Keep the fallback in the same assembly body.
#ifdef GS_REF0
#define GS_LREF0	lda GS_REF0
#else
#define GS_LREF0
#endif
#ifdef GS_REF1
#define GS_LREF1	lda GS_REF1
#else
#define GS_LREF1
#endif
#ifdef GS_REF2
#define GS_LREF2	lda GS_REF2
#else
#define GS_LREF2
#endif
#ifdef GS_REF3
#define GS_LREF3	lda GS_REF3
#else
#define GS_LREF3
#endif
#ifdef GS_REF4
#define GS_LREF4	lda GS_REF4
#else
#define GS_LREF4
#endif
#ifdef GS_REF5
#define GS_LREF5	lda GS_REF5
#else
#define GS_LREF5
#endif
#ifdef GS_REF6
#define GS_LREF6	lda GS_REF6
#else
#define GS_LREF6
#endif
#ifdef GS_REF7
#define GS_LREF7	lda GS_REF7
#else
#define GS_LREF7
#endif
__noinline void gs_blit(void)
{
	__asm volatile
	{
		bit .r
	.r:	lda GS_CODES
		GS_LREF0
		GS_LREF1
		GS_LREF2
		GS_LREF3
		GS_LREF4
		GS_LREF5
		GS_LREF6
		GS_LREF7
		tax
		tay
	}
}

__noinline void gs_blit_list(void)
{
	__asm volatile
	{
		bit .r
	.r:	lda GS_CODES
		GS_LREF0
		GS_LREF1
		GS_LREF2
		GS_LREF3
		GS_LREF4
		GS_LREF5
		GS_LREF6
		GS_LREF7
		tax
		tay
	}
}

// Read the descriptor through a volatile pointer to preserve the runtime
// buffer alignment check. Assembly obtains the entry addresses directly.
static const struct GScreen * volatile gs_d;
static volatile byte gs_a[6];
#define GS_ADR(k)	((volatile byte *)(gs_a[k] | ((word)gs_a[(k) + 1] << 8)))
volatile word gs_list_need;

static void gs_addr(void)
{
	__asm volatile
	{
		lda #<gs_blit
		sta gs_a
		lda #>gs_blit
		sta gs_a + 1
		lda #<gs_blit_list
		sta gs_a + 2
		lda #>gs_blit_list
		sta gs_a + 3
		lda #<gs_core
		sta gs_a + 4
		lda #>gs_core
		sta gs_a + 5
	}
}

// Read the target of entry gs_e in the core jump table.
static byte gs_e;
static word gs_ent(void)
{
	volatile byte * c = GS_ADR(4);
	return c[3 * gs_e + 1] | ((word)c[3 * gs_e + 2] << 8);
}

// Locate the release pass and its pool-capacity operand.
static volatile byte * gs_fin(void)
{
	gs_e = GS_PAGES;
	return (volatile byte *)gs_ent();
}

// Validate the descriptor before writing display or cache state.
static byte gs_check(void)
{
	const struct GScreen * s = gs_d;
	if (!s || !s->slot_of || !s->fixed)
		return GS_E_NULL;
	if (s->dyn != GS_DYN)
		return GS_E_DYN;
	if (s->x0 + GS_W > 40 || s->y0 + GS_H > 24)
		return GS_E_GEOM;
	byte pf = s->pool_first;
#ifdef GS_POOL_LOW
	if (pf < 1 || pf > 127)
#else
	if (pf < 64 || pf > 127)
#endif
		return GS_E_POOL;
	const byte * so = s->slot_of;
	if (s->blank >= GS_DYN || so[s->blank] != 0)
		return GS_E_BLANK;
	for (byte k = 0; k < GS_DYN; k++)
		if (so[k] >= pf && so[k] != 0xff)
			return GS_E_SLOT;
	return GS_OK;
}

// Check the instructions and patch sites before changing executable code.
static byte gs_core_ok(void)
{
	volatile byte * b = GS_ADR(0), * l = GS_ADR(2), * c = GS_ADR(4);
	// Both public entries start with BIT or JMP followed by their fallback.
	if ((b[0] != 0x2c && b[0] != 0x4c) || (l[0] != 0x2c && l[0] != 0x4c))
		return GS_E_CORE;
	for (byte e = 0; e < GS_PAGES + 2; e++)
		if (c[3 * e] != 0x4c)
			return GS_E_CORE;
	if (gs_fin()[0] != 0xa9)
		return GS_E_CORE;
	gs_e = GS_PAGES + 1;
	volatile byte * a = (volatile byte *)gs_ent();
	word l1 = (word)l + 1;
	if (a[0] != 0xa9 || a[2] != 0x8d || a[5] != 0xa9 || a[7] != 0x8d || a[10] != 0x4c)
		return GS_E_CORE;
	if ((a[3] | ((word)a[4] << 8)) != l1 || (a[8] | ((word)a[9] << 8)) != l1 + 1)
		return GS_E_CORE;
	return GS_OK;
}

// Set the operand of gs_blit (0) or gs_blit_list (2).
static word gs_tgt;
static void gs_patch(byte k)
{
	volatile byte * e = GS_ADR(k);
	e[1] = (byte)gs_tgt;
	e[2] = (byte)(gs_tgt >> 8);
}

// Load the fixed glyphs, clear the screen and cache state, and fill the
// free stack so the first dynamic allocation takes pool_first.
static void gs_setup(void)
{
	const struct GScreen * s = gs_d;
	const byte * fx = s->fixed;
	byte pf = s->pool_first;
	word nf = (word)pf * 8;
	for (word i = 0; i < 1024; i++)
		gs_font[i] = (i >= 8 && i < nf) ? fx[i] : 0;
	for (word i = 0; i < 960; i++)
		gs_mem[i] = 0;
	byte bl = s->blank;
	for (word i = 0; i < GS_CELLS; i++)
		gs_prev[i] = bl;
	const byte * so = s->slot_of;
	for (word k = 0; k < 256; k++)
		gs_slot[k] = k < GS_DYN ? so[k] : 0xff;
	for (word i = 0; i < 512; i++)
		gs_ref[i] = 0;
	for (byte i = 0; i < 128; i++)
		gs_owner[i] = 0;
	byte n = 0;
	for (byte sl = 127; sl >= pf; sl--)		// Push in descending order so the first pop gets pool_first.
	{
		gs_free[n] = sl;					// Separate the store and increment for Oscar64 at -O0.
		n++;
	}
	gs_free_n = n;
	gs_pend_n = 0;
	gs_pend_full = 0;
	gs_err_pool = 0;
	gs_err_code = 0;
	gs_pool_peak = 0;
	gs_blankv = bl;
	word i = 0;
	word row = (word)gs_mem + (word)s->y0 * 40 + s->x0;
	for (byte r = 0; r < GS_H; r++)
	{
		for (byte c = 0; c < GS_W; c++)
		{
			word d = row + c - (byte)i;
			gs_dlo[i] = (byte)d;
			gs_dhi[i] = (byte)(d >> 8);
			i++;
		}
		row += 40;
	}
	gs_fin()[1] = 128 - pf;
#ifdef GS_AUDIT
	gs_audit = 0;
	gs_rel_mid = 0;
	gs_aud_nl = 0;
	gs_aud_nh = 0;
	gs_aud_fn = gs_free_n;
#endif
}

// List emitter. Helpers share state to keep their argument frames small.
#define GO_ORA	0x0d	// ora abs
#define GO_BPL	0x10
#define GO_CLC	0x18
#define GO_JSR	0x20
#define GO_BMI	0x30
#define GO_SEC	0x38
#define GO_JMP	0x4c
#define GO_ADCX	0x7d	// adc abs,x
#define GO_STYZ	0x84	// sty zp
#define GO_STAZ	0x85	// sta zp
#define GO_TXA	0x8a
#define GO_STA	0x8d	// sta abs
#define GO_STX	0x8e	// stx abs
#define GO_BCC	0x90
#define GO_LDYI	0xa0	// ldy #
#define GO_LDAZ	0xa5	// lda zp
#define GO_TAY	0xa8
#define GO_TAX	0xaa
#define GO_LDA	0xad	// lda abs
#define GO_LDX	0xae	// ldx abs
#define GO_BCS	0xb0
#define GO_LDAY	0xb9	// lda abs,y
#define GO_LDAX	0xbd	// lda abs,x
#define GO_CPYI	0xc0	// cpy #
#define GO_CMPI	0xc9	// cmp #
#define GO_CMP	0xcd	// cmp abs
#define GO_BNE	0xd0
#define GO_CMPY	0xd9	// cmp abs,y
#define GO_SBCI	0xe9	// sbc #
#define GO_BEQ	0xf0
#define GE_FX	10		// Pending forward branches and jumps to one label.

static const struct GSList * ge_l;		// List descriptor used during initialization.
static const struct GSActor * ge_a;		// Actor being emitted.
static word ge_k;						// Index of the fixed cell being emitted.
static volatile byte * ge_p, * ge_end;	// Cursor and limit for writes to executable RAM.
static byte ge_bad;						// Emitter failure flag.
static byte ge_op, ge_b;				// Opcode and output byte.
static word ge_v;						// Instruction operand.
static volatile byte * ge_q;			// Instruction whose target is being patched.
static volatile byte * ge_fx[GE_FX];				// Instructions waiting for the next label.
static byte ge_fxn;
static word ge_ent[GS_PAGES];			// Core handler address for each page.
static word ge_need;					// Exact byte count found during validation.

static void ge_byte(void)
{
	if (ge_p != ge_end)
		*ge_p++ = ge_b;
	else
		ge_bad = 1;
}

static void ge_e1(void)
{
	ge_b = ge_op;
	ge_byte();
}

static void ge_e2(void)
{
	ge_e1();
	ge_b = (byte)ge_v;
	ge_byte();
}

static void ge_e3(void)
{
	ge_e2();
	ge_b = (byte)(ge_v >> 8);
	ge_byte();
}

#define E1(o)		(ge_op = (o), ge_e1())
#define E2(o, v)	(ge_op = (o), ge_v = (v), ge_e2())
#define E3(o, a)	(ge_op = (o), ge_v = (a), ge_e3())

// Patch an instruction to target the cursor. Leave it alone after an emitter error.
static void ge_fix(void)
{
	volatile byte * q = ge_q;
	if (ge_bad)
		return;
	word t = (word)ge_p;
	if (q[0] == GO_JMP)
	{
		q[1] = (byte)t;
		q[2] = (byte)(t >> 8);
	}
	else if (t - (word)q - 2 > 127)
		ge_bad = 1;
	else
		q[1] = (byte)(t - (word)q - 2);
}

// Emit a forward branch or jump to be resolved by ge_at.
static void ge_fwd(void)
{
	if (ge_fxn < GE_FX)
	{
		ge_fx[ge_fxn] = ge_p;				// Keep the increment separate from the indexed store.
		ge_fxn++;
	}
	else
		ge_bad = 1;
	ge_v = 0;
	if (ge_op == GO_JMP)
		ge_e3();
	else
		ge_e2();
}
#define FW(o)	(ge_op = (o), ge_fwd())

static void ge_at(void)
{
	for (byte i = 0; i < ge_fxn; i++)
	{
		ge_q = ge_fx[i];
		ge_fix();
	}
	ge_fxn = 0;
}

// Emit a fixed-cell visit. A valid fixed-to-fixed change can write the
// slot directly. Other changes use the per-page handler.
static void ge_cell(void)
{
	word k = ge_k, pv = (word)gs_prev + k;
	E3(GO_LDA, (word)GS_CODES + k);
	E3(GO_CMP, pv);
	FW(GO_BEQ);
	E1(GO_TAX);
#if GS_DYN == 0x80
	E3(GO_ORA, pv);
	volatile byte * s = ge_p;
	E2(GO_BMI, 0);
#else
	E2(GO_CMPI, GS_DYN);
	volatile byte * s = ge_p;
	E2(GO_BCS, 0);
	E3(GO_LDA, pv);
	E2(GO_CMPI, GS_DYN);
	volatile byte * s2 = ge_p;
	E2(GO_BCS, 0);
#endif
	E3(GO_LDAX, (word)gs_slot);
	volatile byte * q = ge_p;
	E2(GO_BPL, 0);
	ge_q = s;
	ge_fix();
#if GS_DYN != 0x80
	ge_q = s2;
	ge_fix();
#endif
	E1(GO_TXA);
	E2(GO_LDYI, (byte)k);
	E3(GO_JSR, ge_ent[k >> 8]);
	FW(GO_JMP);
	ge_q = q;
	ge_fix();
	E3(GO_STX, pv);
	word r = k / GS_W;
	E3(GO_STA, (word)gs_mem + (gs_d->y0 + r) * 40 + gs_d->x0 + (k - r * GS_W));
	ge_at();
}

// Emit a bounds check and visit. A contains the page and Y the cell index.
// Skip pages beyond the field and indices beyond the partial final page.
static void ge_chk(void)
{
#if GS_PAGES == 1
	FW(GO_BNE);
#if GS_LAST < 256
	E2(GO_CPYI, GS_LAST);
	FW(GO_BCS);
#endif
	E3(GO_LDAY, (word)GS_CODES);
	E3(GO_CMPY, (word)gs_prev);
	FW(GO_BEQ);
	E3(GO_JSR, ge_ent[0]);
#else
	volatile byte * h = ge_p;
	E2(GO_BNE, 0);
	E3(GO_LDAY, (word)GS_CODES);
	E3(GO_CMPY, (word)gs_prev);
	FW(GO_BEQ);
	E3(GO_JSR, ge_ent[0]);
	FW(GO_JMP);
#if GS_PAGES > 2
	for (byte p = 1; p < GS_PAGES - 1; p++)
	{
		ge_q = h;
		ge_fix();
		E2(GO_CMPI, p);
		h = ge_p;
		E2(GO_BNE, 0);
		E3(GO_LDAY, (word)GS_CODES + 256 * p);
		E3(GO_CMPY, (word)gs_prev + 256 * p);
		FW(GO_BEQ);
		E3(GO_JSR, ge_ent[p]);
		FW(GO_JMP);
	}
#endif
	ge_q = h;
	ge_fix();
	E2(GO_CMPI, GS_PAGES - 1);
	FW(GO_BNE);
#if GS_LAST < 256
	E2(GO_CPYI, GS_LAST);
	FW(GO_BCS);
#endif
	E3(GO_LDAY, (word)GS_CODES + 256 * (GS_PAGES - 1));
	E3(GO_CMPY, (word)gs_prev + 256 * (GS_PAGES - 1));
	FW(GO_BEQ);
	E3(GO_JSR, ge_ent[GS_PAGES - 1]);
#endif
	ge_at();
}

// Emit an actor visit from its offset into the code buffer. If the actor
// has a tail, add the offset selected by its live direction and visit again.
static void ge_actor(void)
{
	word p = (word)ge_a->pos;
	E3(GO_LDA, p);
	E1(GO_SEC);
	E2(GO_SBCI, (byte)(word)GS_CODES);
	E1(GO_TAY);
	E3(GO_LDA, p + 1);
	E2(GO_SBCI, (byte)((word)GS_CODES >> 8));
	E2(GO_STYZ, (byte)(word)&gs_zlo);
	E2(GO_STAZ, (byte)(word)&gs_zhi);
	ge_chk();
	if (ge_a->dir)
	{
		E3(GO_LDX, (word)ge_a->dir);
		E2(GO_LDAZ, (byte)(word)&gs_zlo);
		E1(GO_CLC);
		E3(GO_ADCX, (word)ge_l->tlo);
		E1(GO_TAY);
		E2(GO_LDAZ, (byte)(word)&gs_zhi);
		E3(GO_ADCX, (word)ge_l->thi);
		ge_chk();
	}
}

// Validate the list and count its code bytes before emitting anything.
static byte gs_lcheck(void)
{
	const struct GSList * l = ge_l;
	if (l->ncells && !l->cells)
		return GS_E_LCELL;
	for (byte i = 0; i < l->ncells; i++)
		if (l->cells[i] >= GS_CELLS)
			return GS_E_LCELL;
	if (l->ncnt > GS_LIST_MAXCNT)
		return GS_E_LACT;
	word na = (word)l->nfix + l->ncnt;
	if ((na && !l->act) || (l->ncnt && !l->count))
		return GS_E_LACT;
	word n = (word)l->ncells * GS_LCELL_B + 3;
	if (l->ncnt)
		n += 13 + 7 * (l->ncnt - 1);
	for (word i = 0; i < na; i++)
	{
		const struct GSActor * a = l->act + i;
		if (!a->pos)
			return GS_E_LACT;
		n += GS_LACT_B;
		if (a->dir)
		{
			if (!l->tlo || !l->thi || !l->tlen)
				return GS_E_LACT;
			n += GS_LTAIL_B;
		}
	}
	ge_need = n;
	return GS_OK;
}

// Emit fixed cells and actors, then a dispatch ladder for counted actors.
// The ladder enters their descending fall-through sequence at the active
// count. The generated code ends with a jump to the core release pass.
static byte gs_gen(void)
{
	const struct GSList * l = ge_l;
	for (byte p = 0; p < GS_PAGES; p++)
	{
		gs_e = p;
		ge_ent[p] = gs_ent();
	}
	ge_p = gs_d->code;
	ge_end = ge_p + ge_need;
	ge_bad = 0;
	ge_fxn = 0;
	for (byte i = 0; i < l->ncells; i++)
	{
		ge_k = l->cells[i];
		ge_cell();
	}
	for (byte i = 0; i < l->nfix; i++)
	{
		ge_a = l->act + i;
		ge_actor();
	}
	byte nd = l->ncnt;
	if (nd)
	{
		volatile byte * ch = ge_p;
		E3(GO_LDA, (word)l->count);
		E2(GO_CMPI, nd);
		volatile byte * b = ge_p;
		E2(GO_BCC, 0);
		E3(GO_JMP, 0);						// Jump to counted actor nd - 1.
		for (byte k = nd - 1; k != 0; k--)
		{
			ge_q = b;
			ge_fix();
			E2(GO_CMPI, k);
			b = ge_p;
			E2(GO_BNE, 0);
			E3(GO_JMP, 0);					// Jump to counted actor k - 1.
		}
		ge_q = b;
		ge_fix();
		volatile byte * e = ge_p;
		E3(GO_JMP, 0);						// A count of zero jumps to the end.
		for (byte k = nd; k != 0; k--)
		{
			ge_q = k == nd ? ch + 7 : ch + 14 + 7 * (nd - 1 - k);
			ge_fix();
			ge_a = l->act + l->nfix + (k - 1);
			ge_actor();
		}
		ge_q = e;
		ge_fix();
	}
	gs_e = GS_PAGES;
	E3(GO_JMP, gs_ent());
	if (ge_p != ge_end)
		ge_bad = 1;
	return ge_bad;
}

byte gs_init(const struct GScreen * s)
{
	gs_d = s;							// Use this pointer for subsequent descriptor reads.
	gs_addr();
	byte core = gs_core_ok();
	byte st = gs_check();
	if (!st)
		st = core;
	byte w = GS_W_NOLIST;
	if (!st && gs_d->list)
	{
		ge_l = gs_d->list;
		st = gs_lcheck();
		if (!st)
		{
			gs_list_need = ge_need;
			if (!gs_d->code || gs_d->code_size < ge_need)
				w = GS_W_LISTBUF;
			else if ((byte)(word)gs_d->code)
				w = GS_W_LISTALIGN;
			else if (gs_gen())
				st = GS_E_LGEN;
			else
				w = GS_OK;
		}
	}
	else if (!st)
		gs_list_need = 0;
	if (st)
	{
		if (!core)						// Patch only when the original instructions were recognized.
		{
			gs_tgt = (word)GS_ADR(0) + 3;	// Redirect to the static fallback.
			gs_patch(0);
			gs_tgt = (word)GS_ADR(2) + 3;
			gs_patch(2);
		}
		return st;
	}
	gs_setup();
	gs_e = GS_PAGES + 1;
	gs_tgt = gs_ent();					// Full blit and list setup entry.
	volatile byte * a = (volatile byte *)gs_tgt;
	word t = w ? gs_tgt : (word)gs_d->code;	// Target used by the list entry after a full blit.
	a[1] = (byte)t;
	a[6] = (byte)(t >> 8);
	gs_patch(0);
	gs_patch(2);
	GS_ADR(0)[0] = 0x4c;				// Install JMP after both operands are valid.
	GS_ADR(2)[0] = 0x4c;
	return w;
}
