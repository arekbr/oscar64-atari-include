#include "xmem.h"

// While PORTB selects banked RAM for the CPU, xm_code uses only registers,
// the hardware stack and its own zero-page state. BSS can lie in the
// bank window, and an Oscar64 interrupt can clobber compiler temporaries
// such as tmpy. The copy and probe leave the interrupt state alone.

#define XM_PORTB	(*(volatile __memmap byte *)0xd301)
#define XM_PBCTL	(*(volatile byte *)0xd303)

// Maximum xm_code size used for the layout check.
#define XM_CODE_MAX	256

byte xmem_banks, xmem_caps;

__zeropage byte * xm_src;		// Put reads main RAM and get reads the window. Both subtract y0.
__zeropage byte * xm_dst;
__zeropage byte xm_pbw;		// Entry PORTB value with main RAM visible to the CPU.
__zeropage byte xm_pbb;		// Bank PORTB value. Probe starts at bank 0 with bit 5 set.
__zeropage byte xm_pages;		// Page count. The first page is partial when y0 is nonzero.
__zeropage byte xm_y0;		// First Y index, (256 - n % 256) % 256.
__zeropage byte xm_res;		// Matching probe reads. Five means four separate banks.
__zeropage byte xm_cap;		// Set if bit 5 leaves the CPU bank unchanged.

#pragma code(xmemcode)

// A selects put (0), get (1) or detect (2). X holds bank PORTB for put
// and entry PORTB for get. Copy starts at Y = y0. Return with PORTB = xm_pbw.
__asm xm_code
{
		cmp	#1
		beq	xm_get
		bcs	xm_probe
xm_put:
		lda	(xm_src), y		// Main RAM (entry PORTB).
		stx	$d301			// Bank PORTB.
		sta	(xm_dst), y
		lda	xm_pbw
		sta	$d301			// Entry PORTB.
		iny
		bne	xm_put
		inc	xm_src + 1
		inc	xm_dst + 1
		dec	xm_pages
		bne	xm_put
		rts
xm_get:
		lda	xm_pbb
		sta	$d301			// Bank PORTB.
		lda	(xm_src), y
		stx	$d301			// Entry PORTB.
		sta	(xm_dst), y		// Main RAM.
		iny
		bne	xm_get
		inc	xm_src + 1
		inc	xm_dst + 1
		dec	xm_pages
		bne	xm_get
		rts

// Save six probe bytes on the hardware stack. Save main RAM and the
// four banks before writing markers so restoration also works when they alias.
xm_probe:
		ldx	#12				// bank 3, 2, 1, 0 (X = bank << 2)
xm_p1:
		txa
		ora	xm_pbb
		sta	$d301
		lda	$4000
		pha
		dex
		dex
		dex
		dex
		bpl	xm_p1
		lda	xm_pbw
		sta	$d301
		lda	$4000
		pha					// Main RAM byte on top of the stack.
		ldx	#12				// Bank markers: $A1 | bank << 2 = $AD $A9 $A5 $A1
xm_p2:
		txa
		ora	xm_pbb
		sta	$d301
		txa
		ora	#$a1
		sta	$4000
		dex
		dex
		dex
		dex
		bpl	xm_p2
		lda	xm_pbw
		sta	$d301
		lda	#$5e			// Main RAM marker.
		sta	$4000
		ldx	#12
xm_p3:
		txa
		ora	xm_pbb
		sta	$d301
		txa
		ora	#$a1
		cmp	$4000
		bne	xm_p3n
		inc	xm_res
xm_p3n:
		dex
		dex
		dex
		dex
		bpl	xm_p3
		lda	xm_pbw
		sta	$d301
		lda	$4000
		cmp	#$5e
		bne	xm_p4
		inc	xm_res
xm_p4:
		// Write two markers with bit 5 set and check whether both read back with bit 5 clear.
		lda	xm_pbb
		and	#$df
		sta	$d301
		lda	$4000
		pha					// Save the byte with bit 5 clear (a separate bank on RAMBO).
		lda	xm_pbb
		sta	$d301
		lda	#$33
		sta	$4000
		lda	xm_pbb
		and	#$df
		sta	$d301
		lda	$4000
		cmp	#$33
		bne	xm_p5
		lda	xm_pbb
		sta	$d301
		lda	#$cc
		sta	$4000
		lda	xm_pbb
		and	#$df
		sta	$d301
		lda	$4000
		cmp	#$cc
		bne	xm_p5
		inc	xm_cap
xm_p5:
		lda	xm_pbb
		and	#$df
		sta	$d301
		pla
		sta	$4000
		lda	xm_pbw
		sta	$d301
		pla
		sta	$4000			// Main RAM.
		ldx	#0
xm_p6:
		txa
		ora	xm_pbb
		sta	$d301
		pla
		sta	$4000			// Restore banks 0 through 3.
		inx
		inx
		inx
		inx
		cpx	#16
		bne	xm_p6
		lda	xm_pbw
		sta	$d301
		rts
}

#pragma code(code)

// Read the linker address through assembly. Oscar64 443e323 could drop
// the upper bound when folding a range comparison of a linker address.
static volatile byte xm_alo, xm_ahi;

// E_PIA -> E_LAYOUT -> E_MAPPED
static byte xm_pre(void)
{
	if (!(XM_PBCTL & 0x04))
		return XMEM_E_PIA;
	__asm volatile {
		lda	#<xm_code
		sta	xm_alo
		lda	#>xm_code
		sta	xm_ahi
	}
	word a = xm_alo | ((word)xm_ahi << 8);
	if (a < 0x8000 && a + (XM_CODE_MAX - 1) >= 0x4000)
		return XMEM_E_LAYOUT;
	if (!(XM_PORTB & 0x10))
		return XMEM_E_MAPPED;
	return XMEM_OK;
}

// Only the vector entry addresses are checked. XL OS rev. 2 and 3 also
// dispatch through the timer vectors at $0226 and $0228 and through VPIRQ
// at $0238. IRQ dispatch reaches BRKKY at $0236 through an offset table.
static bool xm_vec(void)
{
	volatile byte * v = (volatile byte *)0x0200;
	for (byte i = 1; i < 0x18; i += 2)		// VDSLST $0200, VPRCED through VIMIRQ $0202-$0217
		if ((v[i] & 0xc0) == 0x40)
			return true;
	for (byte i = 0x23; i < 0x2a; i += 2)	// VVBLKI $0222, VVBLKD $0224, CDTMA1 $0226, CDTMA2 $0228
		if ((v[i] & 0xc0) == 0x40)
			return true;
	for (byte i = 0x37; i < 0x3a; i += 2)	// BRKKY $0236, VPIRQ $0238
		if ((v[i] & 0xc0) == 0x40)
			return true;
	return false;
}

byte xmem_detect(void)
{
	xmem_banks = 0;
	xmem_caps = 0;
	byte s = xm_pre();
	if (s)
		return s;
	if (xm_vec())
		return XMEM_E_VECTOR;
	xm_pbw = XM_PORTB;
	xm_pbb = (xm_pbw & 0x43) | 0xa0;		// Bank 0 for CPU, bit 5 set, self-test ROM off.
	xm_res = 0;
	xm_cap = 0;
	__asm volatile {
		lda	#2
		jsr	xm_code
	}
	if (xm_res == 5)
	{
		xmem_banks = 4;
		xmem_caps = xm_cap ? XMEM_CAP_ANTIC : 0;
	}
	return XMEM_OK;
}

// Keep copy arguments in static storage to avoid a 12-byte fastcall
// frame on older Oscar64 versions. This setup runs with main RAM
// visible, so the arguments may live in BSS inside the window.
static byte xm_bank, xm_dir;
static word xm_off, xm_n;
static byte * xm_buf;

// E_PIA -> E_LAYOUT -> E_MAPPED -> E_BANK -> E_RANGE -> n == 0 = OK -> E_VECTOR, then copy (xm_dir: 0 put, 1 get)
static byte xm_copy(void)
{
	byte s = xm_pre();
	if (s)
		return s;
	if (xm_bank >= xmem_banks)
		return XMEM_E_BANK;
	if (xm_off > XMEM_SIZE || xm_n > XMEM_SIZE - xm_off)
		return XMEM_E_RANGE;
	if (!xm_n)
		return XMEM_OK;
	if (xm_vec())
		return XMEM_E_VECTOR;
	byte w = XM_PORTB;
	xm_pbw = w;
	byte b = (w & 0x43) | 0x80 | (xm_bank << 2);
	if (xmem_caps & XMEM_CAP_ANTIC)
		b |= w & 0x20;		// Preserve the caller's ANTIC bank-enable bit.
	else
		b |= 0x20;			// Keep bank bit 5 set on CPU-only expansions.
	xm_pbb = b;
	byte y0 = (byte)(0 - (byte)xm_n);	// Bias both pointers so the partial first page ends when Y wraps.
	xm_y0 = y0;
	xm_pages = (byte)(xm_n >> 8) + (y0 ? 1 : 0);
	byte * win = (byte *)(XMEM_BASE + xm_off) - y0;
	byte * buf = xm_buf - y0;
	if (xm_dir)
	{
		xm_src = win;
		xm_dst = buf;
		__asm volatile {
			lda	#1
			ldx	xm_pbw
			ldy	xm_y0
			jsr	xm_code
		}
	}
	else
	{
		xm_src = buf;
		xm_dst = win;
		__asm volatile {
			lda	#0
			ldx	xm_pbb
			ldy	xm_y0
			jsr	xm_code
		}
	}
	return XMEM_OK;
}

byte xmem_put(byte bank, word off, const void * src, word n)
{
	xm_bank = bank;
	xm_off = off;
	xm_buf = (byte *)src;
	xm_n = n;
	xm_dir = 0;
	return xm_copy();
}

byte xmem_get(byte bank, word off, void * dst, word n)
{
	xm_bank = bank;
	xm_off = off;
	xm_buf = (byte *)dst;
	xm_n = n;
	xm_dir = 1;
	return xm_copy();
}

byte xmem_map(byte bank, byte who)
{
	if (!(XM_PBCTL & 0x04))
		return XMEM_E_PIA;
	if (!xmem_banks)
		return XMEM_E_BANK;
	if (!who || (who & (byte)~(XMEM_CPU | XMEM_ANTIC)))
		return XMEM_E_ARG;
	if ((who & XMEM_ANTIC) && !(xmem_caps & XMEM_CAP_ANTIC))
		return XMEM_E_ANTIC;
	if (bank >= xmem_banks)
		return XMEM_E_BANK;
	byte v = (XM_PORTB & 0x43) | 0x80 | (bank << 2);
	if (!(who & XMEM_CPU))
		v |= 0x10;
	if (!(who & XMEM_ANTIC))
		v |= 0x20;
	XM_PORTB = v;
	return XMEM_OK;
}

byte xmem_unmap(void)
{
	if (!(XM_PBCTL & 0x04))
		return XMEM_E_PIA;
	XM_PORTB |= 0x30;
	return XMEM_OK;
}
