#include "vpmg.h"
#include "pmg.h"
#include "gtia.h"
#include "antic.h"
#include "os.h"

#define OS_SETVBV	0xe45c

// Zone index is player * 32 + buffer * 16 + zone. VBI handles zone 0.
static volatile byte vt_h[128], vt_c[128];
#pragma align(vt_h, 256)					// Avoid a page crossing and an extra cycle in the DLI.
#pragma align(vt_c, 256)

static volatile byte vp_pending;		// Back buffer ready for the next VBI.
static volatile byte vp_front;			// Displayed buffer, 0 or 1.
static volatile byte vp_base;			// vp_front * 16
static volatile byte vp_idx;			// Current DLI table index.
static volatile byte vp_zone;			// DLIs processed in this frame.
static volatile byte vp_n;				// Zone boundary count.
static volatile byte vp_pmhi[2];		// High bytes of the two PMG buffer addresses.
static volatile word vp_shown;
static word vp_prepared;
static volatile word vp_oldvbi;
#pragma align(vp_oldvbi, 2)				// Keep the indirect jump vector away from the NMOS 6502 page-wrap case.
static volatile byte vp_lo, vp_hi;		// Address bytes for SETVBV.
static bool vp_running;

static volatile byte * vp_pm[2];
static byte vp_lines[VPMG_ZONES];
static byte vp_zidx[256];				// Number of boundaries at or before each line for constant-time lookup.

// Parallel arrays let each field use the virtual player index directly.
static const byte * vs_img[VPMG_MAX];
static byte vs_x[VPMG_MAX], vs_y[VPMG_MAX], vs_h[VPMG_MAX], vs_c[VPMG_MAX];
static bool vs_vis[VPMG_MAX];
static byte vp_own[4 * (VPMG_ZONES + 1)];	// Virtual player index plus one at its first zone.
#define VPMG_FREE	0xff

// Record which image rows each buffer needs cleared before reuse.
static byte vp_hn[2];
static byte vp_hp[2 * VPMG_MAX], vp_hy[2 * VPMG_MAX], vp_hh[2 * VPMG_MAX];	// Buffer b starts at b * VPMG_MAX.

// Set the next zone. Write HPOSP0..3 first without WSYNC, then COLPM0..3.
__asm vpmg_dli
{
		pha
		txa
		pha
		lda	vp_zone
		cmp	vp_n
		bcs	vd_done
		inc	vp_zone
		inc	vp_idx
		ldx	vp_idx
		lda	vt_h + 0, x
		sta	0xd000
		lda	vt_h + 32, x
		sta	0xd001
		lda	vt_h + 64, x
		sta	0xd002
		lda	vt_h + 96, x
		sta	0xd003
		lda	vt_c + 0, x
		sta	0xd012
		lda	vt_c + 32, x
		sta	0xd013
		lda	vt_c + 64, x
		sta	0xd014
		lda	vt_c + 96, x
		sta	0xd015
vd_done:
		pla
		tax
		pla
		rti
}

// Swap PMBASE and the DLI tables together, then apply zone 0 to hardware and OS shadows.
__asm vpmg_vbi
{
		lda	vp_pending
		beq	vv_keep
		lda	vp_front
		eor	#1
		sta	vp_front
		tax
		lda	vp_pmhi, x
		sta	0xd407
		txa
		asl
		asl
		asl
		asl
		sta	vp_base
		lda	#0
		sta	vp_pending
		inc	vp_shown
		bne	vv_keep
		inc	vp_shown + 1
vv_keep:
		ldx	vp_base
		stx	vp_idx
		lda	#0
		sta	vp_zone
		lda	vt_h + 0, x
		sta	0xd000
		lda	vt_h + 32, x
		sta	0xd001
		lda	vt_h + 64, x
		sta	0xd002
		lda	vt_h + 96, x
		sta	0xd003
		lda	vt_c + 0, x
		sta	0xd012
		sta	0x02c0
		lda	vt_c + 32, x
		sta	0xd013
		sta	0x02c1
		lda	vt_c + 64, x
		sta	0xd014
		sta	0x02c2
		lda	vt_c + 96, x
		sta	0xd015
		sta	0x02c3
		jmp	(vp_oldvbi)
}

void vpmg_init(volatile byte * pm0, volatile byte * pm1)
{
	pmg_init(pm1, PMG_1LINE);			// Clear pm1.
	pmg_init(pm0, PMG_1LINE);			// Clear pm0 and select it for display.
	vp_pm[0] = pm0;
	vp_pm[1] = pm1;
	vp_pmhi[0] = (byte)((word)pm0 >> 8);
	vp_pmhi[1] = (byte)((word)pm1 >> 8);
	for (byte i = 0; i < 4; i++)
		gtia.sizep[i] = 0;
	gtia.sizem = 0;
	gtia.vdelay = 0;
	gtia.gractl = GRACTL_PLAYERS;
	os_sdmctl &= (byte)~DMACTL_MISSILES;
	antic.dmactl = os_sdmctl;			// Enable player DMA even when CRITIC prevents the OS from copying the shadow.
	for (byte i = 0; i < VPMG_MAX; i++)
		vs_vis[i] = false;
	vp_hn[0] = vp_hn[1] = 0;
	vp_front = 0;
	vp_base = 0;
	vp_pending = 0;
	vpmg_zones(0, 0);
	vp_shown = 0;
	vp_prepared = 0;
}

bool vpmg_zones(const byte * lines, byte n)
{
	if (vp_running || n > VPMG_ZONES)
		return false;
	for (byte i = 0; i < n; i++)
	{
		if (lines[i] > VPMG_LAST || (i > 0 && lines[i] < lines[i - 1] + VPMG_GUARD + 2))
			return false;
	}
	for (byte i = 0; i < n; i++)
		vp_lines[i] = lines[i];
	vp_n = n;
	// Discard both frame plans, including a pending swap. Keep players
	// hidden until an update prepares a plan for these boundaries.
	vp_pending = 0;
	for (byte i = 0; i < 128; i++)
	{
		vt_h[i] = 0;
		vt_c[i] = 0;
	}
	byte z = 0, y = 0;
	do
	{
		while (z < n && lines[z] <= y)
			z++;
		vp_zidx[y] = z;
	} while (++y);
	return true;
}

void vpmg_set(byte sp, byte x, byte y, const byte * img, byte h, byte color)
{
	vs_img[sp] = img;
	vs_h[sp] = h;
	vs_c[sp] = color;
	vpmg_move(sp, x, y);
}

void vpmg_move(byte sp, byte x, byte y)
{
	byte h = vs_h[sp];
	vs_x[sp] = x;
	vs_y[sp] = y;
	vs_vis[sp] = h >= 1 && h <= VPMG_HMAX && (word)y + h <= 256;
}

void vpmg_hide(byte sp)
{
	vs_vis[sp] = false;
}

byte vpmg_update(void)
{
	if (vp_pending)
		return VPMG_BUSY;

	byte b = vp_front ^ 1;
	volatile byte * pm = vp_pm[b] + 0x400;

	// Clear rows drawn the last time this back buffer was used.
	byte hb = b ? VPMG_MAX : 0, he = hb + vp_hn[b];
	for (byte i = hb; i < he; i++)
	{
		volatile byte * d = pm + ((word)vp_hp[i] << 8) + vp_hy[i];
		byte h = vp_hh[i];
		for (byte k = 0; k < h; k++)
			d[k] = 0;
	}

	// Stable insertion sort by scan line.
	byte ord[VPMG_MAX], cnt = 0;
	for (byte i = 0; i < VPMG_MAX; i++)
	{
		if (vs_vis[i])
		{
			byte y = vs_y[i], j = cnt++;
			while (j > 0 && vs_y[ord[j - 1]] > y)
			{
				ord[j] = ord[j - 1];
				j--;
			}
			ord[j] = i;
		}
	}

	// An entry in vp_own installs a new virtual player at this zone. Zero holds
	// the previous assignment. A used player is available at its first boundary
	// past the prior image when that boundary plus GUARD is at or before y.
	for (byte z = 0; z < 4 * (VPMG_ZONES + 1); z++)
		vp_own[z] = 0;
	byte first[4] = {VPMG_FREE, VPMG_FREE, VPMG_FREE, VPMG_FREE};
	byte hidden = 0, hn = hb;
	for (byte i = 0; i < cnt; i++)
	{
		byte si = ord[i];
		byte y = vs_y[si], h = vs_h[si];
		byte lim = y >= VPMG_GUARD ? vp_zidx[y - VPMG_GUARD] : 0;	// Boundaries whose guard lines end by y.
		byte p = 0, zs = 0;
		for (; p < 4; p++)
		{
			byte f = first[p];
			if (f == VPMG_FREE)
				break;
			if (f < lim)
			{
				zs = f + 1;
				break;
			}
		}
		if (p == 4)
		{
			hidden++;
			continue;
		}
		// The next boundary must be at least two lines past the last image row.
		byte f = (word)y + h > 255 ? vp_n : vp_zidx[(byte)(y + h)];
		first[p] = f;					// vp_n keeps this player assigned through the end of the frame.
		vp_own[(byte)(p * (VPMG_ZONES + 1)) + zs] = si + 1;
		volatile byte * d = pm + ((word)p << 8) + y;
		const byte * src = vs_img[si];
		for (byte k = 0; k < h; k++)
			d[k] = src[k];
		vp_hp[hn] = p;
		vp_hy[hn] = y;
		vp_hh[hn] = h;
		hn++;
	}
	vp_hn[b] = hn - hb;

	// Build the back buffer tables with one pass per hardware player.
	byte oi = 0;
	for (byte p = 0; p < 4; p++)
	{
		byte x = 0, c = 0, t = (p << 5) | (b << 4), te = t + vp_n;
		byte o = oi;
		for (; t <= te; t++)
		{
			byte w = vp_own[o++];
			if (w)
			{
				x = vs_x[w - 1];
				c = vs_c[w - 1];
			}
			vt_h[t] = x;
			vt_c[t] = c;
		}
		oi += VPMG_ZONES + 1;
	}
	vp_prepared++;
	vp_pending = 1;						// Publish only after all frame data is written.
	return hidden;
}

word vpmg_prepared(void)
{
	return vp_prepared;
}

word vpmg_shown(void)
{
	word w;
	do
		w = vp_shown;
	while (w != vp_shown);				// Retry when VBI changes the counter during a read.
	return w;
}

static void setvbv_immediate(void)
{
	__asm {
		lda	#6
		ldy	vp_lo
		ldx	vp_hi
		jsr	OS_SETVBV
	}
}

void vpmg_start(void)
{
	antic.nmien = NMIEN_VBI;
	os_vdslst = (void *)vpmg_dli;
	if (!vp_running)
	{
		// Save the immediate VBI vector for chaining and restoration.
		vp_oldvbi = (word)os_vvblki;
		vp_lo = (byte)(word)vpmg_vbi;
		vp_hi = (byte)((word)vpmg_vbi >> 8);
		setvbv_immediate();
		vp_running = true;
	}
	os_wait_frame();
	antic.nmien = NMIEN_VBI | NMIEN_DLI;
}

void vpmg_stop(void)
{
	antic.nmien = NMIEN_VBI;
	if (vp_running)
	{
		vp_lo = (byte)vp_oldvbi;
		vp_hi = (byte)(vp_oldvbi >> 8);
		setvbv_immediate();
		vp_running = false;
	}
}
