#include "input.h"

byte joy_read(byte n)
{
	return ~os_stick[n & 1] & 0x0f;
}

bool joy_fire(byte n)
{
	return os_strig[n & 1] == 0;
}

byte pad_read(byte n)
{
	return os_paddl[n & 3];
}

bool pad_fire(byte n)
{
	return os_ptrig[n & 3] == 0;
}

byte cons_keys(void)
{
	return ~gtiar.consol & (CONSOL_START | CONSOL_SELECT | CONSOL_OPTION);
}

// Read and clear the HELP latch at $02DC with IRQs masked.
byte key_help(void)
{
	return __asm {
		php
		sei
		lda	0x02dc
		ldy	#0
		sty	0x02dc
		plp
		sta	accu
	};
}

bool key_pressed(void)
{
	return os_ch != KEY_NONE;
}

byte key_get(void)
{
	byte k;
	while ((k = os_ch) == KEY_NONE)
		;
	os_ch = KEY_NONE;
	return k;
}

// The GET entry is at offset 4 in the K: table at $E420.
// It holds the handler address minus one. RTS transfers control to it.
// Set ICAX1Z ($2A) to 12 for a blocking read.
// A set low bit can make the ROM handler at $F302 return $9B immediately.
// The handler returns to the original caller.
__asm key_kget
{
		lda	#12
		sta	0x2a
		lda	0xe425
		pha
		lda	0xe424
		pha
}

byte key_getc(void)
{
	return __asm {
		jsr	key_kget
		sta	accu
	};
}

byte key_atascii(byte code)
{
	// Follow the XL/XE ROM translation beginning at $F2FD.
	// With Caps Lock set, look up unmodified letters again using SHFLOK.
	// Codes $80-$8D are OS commands and do not produce characters.
	// Codes $8E-$91 map to the ATASCII cursor controls $1C-$1F.
	if (code >= 0xc0)
		return 0;
	byte c = os_keydef[code];
	if (code < 0x40 && c >= 'a' && c <= 'z' && os_shflok)
		c = os_keydef[code | os_shflok];
	if (c >= 0x80 && c < 0x92)
		return c >= 0x8e ? c - 0x72 : 0;
	return c;
}
