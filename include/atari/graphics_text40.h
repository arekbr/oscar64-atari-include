#ifndef ATARI_GRAPHICS_TEXT40_H
#define ATARI_GRAPHICS_TEXT40_H

#include "antic.h"

struct AGraphicsText40
{
	volatile byte * list, * text, * fonts;
	const byte * font;
	word list_size, text_size;
	byte rows, top;
};

// Install six-scanline rows of 40 ANTIC 2 characters. Supply a 1 KiB
// source font and 2 KiB-aligned storage for two generated font pages.
// The caller may reuse its own console drawing routines on the text buffer.
// Opening this mode takes the DLI and deferred VBI hooks.
bool gr_text40_show(const AGraphicsText40 * screen);

// Stop the mode and restore the previous display and character base.
// The DLI and deferred VBI hooks are released for the caller.
void gr_text40_off(void);

#pragma compile("graphics_text40.c")

#endif
