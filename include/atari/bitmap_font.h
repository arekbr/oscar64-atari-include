#ifndef ATARI_BITMAP_FONT_H
#define ATARI_BITMAP_FONT_H

#include "bitmap_canvas.h"
#include "bitmap_image.h"

// Normalize 128 eight-row glyphs in screen-code order in place and
// generate the proportional widths. Blank glyphs advance three pixels.
// Supply disjoint glyph (1024 B) and width (128 B) storage and retain
// both after binding the font.
bool abm_font_prepare(ABitmapFont * font, byte * glyphs, byte * widths, byte spacing);

// Read a raw 1024-byte Atari FNT through CIO and prepare it in file->work.
byte abm_font_load(ABitmapFont * font, const ABitmapImageFile * file, byte * widths, byte spacing);

#pragma compile("bitmap_font.c")

#endif
