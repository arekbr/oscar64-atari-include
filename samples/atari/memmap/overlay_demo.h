#ifndef ATARI_SAMPLE_OVERLAY_H
#define ATARI_SAMPLE_OVERLAY_H

#include <atari/console.h>

#define ENTRY_ADDRESS 0x8000
#define ENTRY_SIZE 6
#define MODULE_SIZE 63
#define OVERLAYS 6
#define overlay_code ((byte *)ENTRY_ADDRESS)

// The code and string share one load window. The module passes its string
// offset back in the last byte of the window.
static void print_overlay(void)
{
	volatile byte * offset = (volatile byte *)(ENTRY_ADDRESS + MODULE_SIZE);
	*offset = 0;
	__asm {
		jsr ENTRY_ADDRESS
	}
	byte start = *offset;
	if (start == ENTRY_SIZE)
		con_printf("%s\n", (const char *)overlay_code + start);
}

#endif
