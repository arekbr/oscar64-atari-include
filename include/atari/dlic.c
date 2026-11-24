#include "dlic.h"
#include "antic.h"
#include "os.h"

#define OS_SETVBV		0xe45c
#define SETVBV_IMMEDIATE	6

#define OP_PHA	0x48
#define OP_PLA	0x68
#define OP_TXA	0x8a
#define OP_TAX	0xaa
#define OP_TYA	0x98
#define OP_TAY	0xa8
#define OP_LDA	0xa9
#define OP_STA	0x8d
#define OP_JSR	0x20
#define OP_RTI	0x40

#define NEXT_LO	2		// Operand of LDA #next.

static byte dlic_pool[256];				// Slot code with the empty RTI handler at offset zero.
#pragma align(dlic_pool, 256)

static volatile byte dlic_first;		// Low byte of the first slot address.
static volatile word dlic_oldvbi;		// Saved immediate VBI vector.
#pragma align(dlic_oldvbi, 2)			// Keep the indirect JMP vector clear of a page boundary.
static volatile word dlic_tgt;			// Callback address patched by the current slot.
#pragma align(dlic_tgt, 2)
static volatile byte dlic_lo, dlic_hi;	// SETVBV arguments shared with inline assembly.
static byte dlic_free;					// Next free offset, or zero before initialization.
static bool dlic_running;

#define POOL	((volatile byte *)dlic_pool)

// Rewind VDSLST, then chain to the saved immediate VBI handler.
__asm dlic_vbi
{
		lda	dlic_first
		sta	0x0200
		jmp	(dlic_oldvbi)
}

__asm dlic_jmp
{
		jmp	(dlic_tgt)
}

// Preserve the runtime workspace that __interrupt can miss across calls.
// Clear decimal mode before executing C code in the callback.
__asm dlic_wrap
{
		lda	__tmpy	// tmpy
		pha				// tmpy
		lda	__tmp + 4	// work
		pha				// work
		lda	__tmp + 5	// work
		pha				// work
		lda	__tmp + 6	// work
		pha				// work
		lda	__tmp + 7	// work
		pha				// work
		lda	__tmp + 8	// work
		pha				// work
		cld
		jsr	dlic_jmp
		pla				// work
		sta	__tmp + 8	// work
		pla				// work
		sta	__tmp + 7	// work
		pla				// work
		sta	__tmp + 6	// work
		pla				// work
		sta	__tmp + 5	// work
		pla				// work
		sta	__tmp + 4	// work
		pla				// tmpy
		sta	__tmpy	// tmpy
		rts
}

static void emit(DLISlot * s, byte b)
{
	if (s->len < DLIC_CODE)
		POOL[s->at + s->len] = b;
	else
		s->bad = true;
	s->len++;
}

static inline void emit_sta(DLISlot * s, word addr)
{
	emit(s, OP_STA);
	emit(s, (byte)addr);
	emit(s, (byte)(addr >> 8));
}

bool dlic_begin(DLISlot * s, bool wsync)
{
	if (!dlic_free)
	{
		POOL[0] = OP_RTI;
		dlic_free = 1;
	}
	if (!s->at)
	{
		if (dlic_free > 256 - DLIC_CODE)
			return false;
		s->at = dlic_free;
		dlic_free += DLIC_CODE;
	}
	s->len = 0;
	s->nops = 0;
	s->calls = 0;
	s->bad = false;
	emit(s, OP_PHA);
	emit(s, OP_LDA);
	emit(s, 0);							// dlic_chain fills in the next slot offset.
	emit_sta(s, 0x0200);
	if (wsync)
		emit_sta(s, (word)&antic.wsync);
	return true;
}

static byte new_op(DLISlot * s)
{
	if (!s->at || s->nops >= DLIC_OPS)
	{
		s->bad = true;
		return DLIC_FULL;
	}
	byte op = s->nops++;
	s->ofs[op] = s->len;
	return op;
}

byte dlic_write(DLISlot * s, volatile void * addr, byte data)
{
	byte op = new_op(s);
	if (op != DLIC_FULL)
	{
		emit(s, OP_LDA);
		emit(s, data);
		emit_sta(s, (word)addr);
	}
	return s->bad ? DLIC_FULL : op;
}

byte dlic_call(DLISlot * s, IRQHandler f)
{
	byte op = new_op(s);
	if (op != DLIC_FULL)
	{
		s->calls |= 1 << op;
		emit(s, OP_TXA);
		emit(s, OP_PHA);
		emit(s, OP_TYA);
		emit(s, OP_PHA);
		emit(s, OP_LDA); emit(s, (byte)(word)f);
		emit_sta(s, (word)&dlic_tgt);
		emit(s, OP_LDA); emit(s, (byte)((word)f >> 8));
		emit_sta(s, (word)&dlic_tgt + 1);
		emit(s, OP_JSR);
		emit(s, (byte)(word)dlic_wrap);
		emit(s, (byte)((word)dlic_wrap >> 8));
		emit(s, OP_PLA);
		emit(s, OP_TAY);
		emit(s, OP_PLA);
		emit(s, OP_TAX);
	}
	return s->bad ? DLIC_FULL : op;
}

bool dlic_end(DLISlot * s)
{
	if (!s->at)
		return false;
	emit(s, OP_PLA);
	emit(s, OP_RTI);
	return !s->bad;
}

static bool is_write(DLISlot * s, byte op)
{
	return op < s->nops && !(s->calls & (1 << op));
}

bool dlic_data(DLISlot * s, byte op, byte data)
{
	if (!is_write(s, op))
		return false;
	POOL[s->at + s->ofs[op] + 1] = data;
	return true;
}

bool dlic_addr(DLISlot * s, byte op, volatile void * addr)
{
	if (dlic_running || !is_write(s, op))
		return false;
	volatile byte * c = POOL + s->at + s->ofs[op];
	c[3] = (byte)(word)addr;
	c[4] = (byte)((word)addr >> 8);
	return true;
}

void dlic_chain(DLISlot ** slots, byte n)
{
	for (byte i = 1; i < n; i++)
		POOL[slots[i - 1]->at + NEXT_LO] = slots[i]->at;
	if (n)
		POOL[slots[n - 1]->at + NEXT_LO] = 0;
}

static void setvbv_immediate(void)
{
	__asm {
		lda	#SETVBV_IMMEDIATE
		ldy	dlic_lo
		ldx	dlic_hi
		jsr	OS_SETVBV
	}
}

void dlic_start(DLISlot * first)
{
	antic.nmien = NMIEN_VBI;
	dlic_first = first->at;
	os_vdslst = (void *)(dlic_pool + first->at);
	if (!dlic_running)
	{
		dlic_oldvbi = (word)os_vvblki;
		dlic_lo = (byte)(word)dlic_vbi;
		dlic_hi = (byte)((word)dlic_vbi >> 8);
		setvbv_immediate();
		dlic_running = true;
	}
	os_wait_frame();				// Enable DLI at the next frame.
	antic.nmien = NMIEN_VBI | NMIEN_DLI;
}

void dlic_stop(void)
{
	antic.nmien = NMIEN_VBI;
	if (dlic_running)
	{
		dlic_lo = (byte)dlic_oldvbi;
		dlic_hi = (byte)(dlic_oldvbi >> 8);
		setvbv_immediate();
		dlic_running = false;
	}
}
