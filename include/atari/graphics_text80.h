#ifndef ATARI_GRAPHICS_TEXT80_H
#define ATARI_GRAPHICS_TEXT80_H

#include "bitmap.h"

#define GR_TEXT80_COLS	80
#define GR_TEXT80_ROWS	24

// Each cell occupies 4 by 8 pixels of a bound 320 by 192, 1-bit bitmap.
// Supply 128 eight-byte glyphs in screen-code order, with bit 3 at the
// left and bit 0 at the right. The font and text must be outside the bitmap.
// Bit 7 of an ATASCII character inverts the four pixels of each scanline.
// Out-of-bounds cells and invalid layouts leave the bitmap unchanged.
bool gr_text80_put(const ABitmap * bitmap, byte column, byte row, byte ch, const byte * font);

// Draw NUL-terminated ATASCII up to the right edge. Return the number of
// cells written. Use gr_text80_put() to draw a literal zero character.
byte gr_text80_write(const ABitmap * bitmap, byte column, byte row, const char * text, const byte * font);

// Move the bitmap up by whole character rows and clear the exposed rows.
// A count of 24 or more clears all cells. Scanline padding is preserved.
bool gr_text80_scroll(const ABitmap * bitmap, byte rows);

#pragma compile("graphics_text80.c")

#endif
