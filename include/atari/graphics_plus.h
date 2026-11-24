#ifndef ATARI_GRAPHICS_PLUS_H
#define ATARI_GRAPHICS_PLUS_H

#include "antic.h"

#define GR_PLUS_TEXT2 DL_MODE2
#define GR_PLUS_TEXT4 DL_MODE4
#define GR_PLUS_GTIA10 0x10

struct AGraphicsPlus
{
	volatile byte * list, * pixels;
	word list_size, pixel_size;
	byte mode, rows, row_height, top;
};

// Build a normal-width display list with 40 bytes per row, then install it
// and take ownership of DLI. rows must be even. Text heights are 2..6,
// GTIA heights 2..16. Each row must fit within a 4 KB ANTIC page.
// Any failed validation leaves memory and the current display untouched.
bool gr_plus_show(const AGraphicsPlus * screen);

// Change the height of the current rows at a frame boundary. A height that
// would exceed the display is rejected. Leave the display list in place.
bool gr_plus_height(byte height);

// Release the handler and reset VSCROL. The next display owns the list,
// DMA control, CHBASE and GTIA mode.
void gr_plus_off(void);

#pragma compile("graphics_plus.c")

#endif
