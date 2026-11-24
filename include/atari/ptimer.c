#include "ptimer.h"
#include "pokey.h"
#include "os.h"

#define VTIMR	((volatile word *)0x0210)		// VTIMR1 $0210, VTIMR2 $0212, VTIMR4 $0214

static volatile byte ptm_tl[3], ptm_th[3];	// address minus one for dispatch through RTS
static word ptm_old[3];						// saved vector from first attach
static bool ptm_att[3];

static const byte ptm_bit[3] = {IRQ_TIMER1, IRQ_TIMER2, IRQ_TIMER4};

// The OS has stacked A and the timer entry has stacked X.
// The common wrapper saves Y and compiler temporaries before the
// callback and clears decimal mode for its C arithmetic.
__asm ptm_common
{
		lda	__tmpy
		pha
		lda	__tmp + 4
		pha
		lda	__tmp + 5
		pha
		lda	__tmp + 6
		pha
		lda	__tmp + 7
		pha
		lda	__tmp + 8
		pha
		cld
		jsr	ptm_call
		pla
		sta	__tmp + 8
		pla
		sta	__tmp + 7
		pla
		sta	__tmp + 6
		pla
		sta	__tmp + 5
		pla
		sta	__tmp + 4
		pla
		sta	__tmpy
		pla
		tay
		pla
		tax
		pla
		rti
ptm_call:
		lda	ptm_th, x
		pha
		lda	ptm_tl, x
		pha
		rts
}

__asm ptm_irq1
{
		txa
		pha
		tya
		pha
		ldx	#0
		jmp	ptm_common
}

__asm ptm_irq2
{
		txa
		pha
		tya
		pha
		ldx	#1
		jmp	ptm_common
}

__asm ptm_irq4
{
		txa
		pha
		tya
		pha
		ldx	#2
		jmp	ptm_common
}

// Save the caller's status on the stack, where nested calls cannot
// overwrite it. Keep the stack balanced between PHP and PLP.
// Force both helpers inline so the saved status stays in the caller's frame.
static __forceinline void ptm_sei(void)
{
	__asm {
		php
		sei
	}
}

static __forceinline void ptm_restore(void)
{
	__asm {
		plp
	}
}

// Map timer 1, 2 or 4 to its vector index. Return 255 otherwise.
static byte ptm_idx(byte t)
{
	return t == PTIMER_1 ? 0 : t == PTIMER_2 ? 1 : t == PTIMER_4 ? 2 : 255;
}

static void ptm_mask(byte k, bool on)
{
	ptm_sei();
	if (on)
		{ os_pokmsk |= ptm_bit[k]; pokey.irqen = os_pokmsk; }
	else
		{ os_pokmsk &= (byte)~ptm_bit[k]; pokey.irqen = os_pokmsk; }
	ptm_restore();
}

void ptimer_attach(byte t, IRQHandler f)
{
	byte k = ptm_idx(t);
	if (k == 255)
		return;
	word a = (word)f - 1;
	word w = k == 0 ? (word)ptm_irq1 : k == 1 ? (word)ptm_irq2 : (word)ptm_irq4;
	ptm_sei();
	ptm_tl[k] = (byte)a;
	ptm_th[k] = (byte)(a >> 8);
	if (!ptm_att[k])
	{
		ptm_old[k] = VTIMR[k];
		VTIMR[k] = w;
		ptm_att[k] = true;
	}
	ptm_restore();
}

bool ptimer_claim(byte t, IRQHandler f)
{
	byte k = ptm_idx(t);
	if (k == 255 || (word)f == 0)
		return false;
	ptm_sei();
	bool available = !ptm_att[k] && !(os_pokmsk & ptm_bit[k]);
	if (available)
		ptimer_attach(t, f);
	ptm_restore();
	return available;
}

void ptimer_detach(byte t)
{
	byte k = ptm_idx(t);
	if (k == 255 || !ptm_att[k])
		return;
	ptm_mask(k, false);
	ptm_sei();
	VTIMR[k] = ptm_old[k];
	ptm_att[k] = false;
	ptm_restore();
}

void ptimer_enable(byte t)
{
	byte k = ptm_idx(t);
	if (k != 255)
		ptm_mask(k, true);
}

void ptimer_disable(byte t)
{
	byte k = ptm_idx(t);
	if (k != 255)
		ptm_mask(k, false);
}

void ptimer_period(byte t, byte audf)
{
	if (t == PTIMER_1 || t == PTIMER_2)
		pokey.aud[t - 1].audf = audf;
	else if (t == PTIMER_4)
		pokey.aud[3].audf = audf;
}

void ptimer_period16(byte t, word div)
{
	if (t == PTIMER_2)
	{
		ptm_mask(0, false);				// timer 1 forms the low byte of timer 2
		pokey.aud[0].audf = (byte)div;
		pokey.aud[1].audf = (byte)(div >> 8);
	}
	else if (t == PTIMER_4)
	{
		pokey.aud[2].audf = (byte)div;
		pokey.aud[3].audf = (byte)(div >> 8);
	}
}

void ptimer_restart_all(void)
{
	pokey.stimer = 0;
}
