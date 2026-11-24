#include "dlist.h"
#include "os.h"

volatile byte * dl_blank(volatile byte * dl, byte lines)
{
	while (lines > 8)
	{
		*dl++ = DL_BLANK8;
		lines -= 8;
	}
	if (lines)
		*dl++ = DL_BLANK(lines);
	return dl;
}

volatile byte * dl_mode(volatile byte * dl, byte mode, byte count)
{
	while (count--)
		*dl++ = mode;
	return dl;
}

volatile byte * dl_lms(volatile byte * dl, byte mode, const volatile void * screen)
{
	word a = (word)screen;
	dl[0] = mode | DL_LMS;
	dl[1] = (byte)a;
	dl[2] = (byte)(a >> 8);
	return dl + 3;
}

volatile byte * dl_jvb(volatile byte * dl, const volatile void * start)
{
	word a = (word)start;
	dl[0] = DL_JVB;
	dl[1] = (byte)a;
	dl[2] = (byte)(a >> 8);
	return dl + 3;
}

volatile byte * dl_jmp(volatile byte * dl, const volatile void * start)
{
	word a = (word)start;
	dl[0] = DL_JMP;
	dl[1] = (byte)a;
	dl[2] = (byte)(a >> 8);
	return dl + 3;
}

bool dl_is_lms(const volatile byte * dl)
{
	byte b = *dl;
	return (b & 0x0f) >= 2 && (b & DL_LMS);
}

byte dl_size(const volatile byte * dl)
{
	byte b = *dl;
	if ((b & 0x0f) == 1)		// JMP or JVB
		return 3;
	if ((b & 0x0f) && (b & DL_LMS))	// Bit 6 is part of the scanline count in blank instructions.
		return 3;
	return 1;
}

volatile byte * dl_next(volatile byte * dl)
{
	byte b = *dl;
	if ((b & 0x0f) == 1)
	{
		if (b & DL_LMS)			// JVB ends the list.
			return 0;
		return (volatile byte *)(dl[1] | (word)dl[2] << 8);
	}
	return dl + dl_size(dl);
}

void dl_set(const volatile void * dl)
{
	os_wait_frame();
	os_sdlst = (word)dl;
}
