#ifndef ATARI_BITMAP_STROKE_FONT_H
#define ATARI_BITMAP_STROKE_FONT_H

#include "bitmap_canvas.h"
#include "bitmap_image.h"

#define ABM_STROKE_SCALE_ONE 256u

struct ABitmapStrokeFont
{
	const byte * offsets, * widths, * strokes;
	word count, stroke_size;
	byte first;
	int ascender;
};

// Bind the font to an unmodified CHR file in memory. On failure the prior
// binding is unchanged. The file must remain live for the life of the font.
byte abm_stroke_font_bind(ABitmapStrokeFont * font, const byte * data, word size);

// Read a CHR file through CIO into an ABitmapImageFile's retained buffer.
byte abm_stroke_font_load(ABitmapStrokeFont * font, const ABitmapImageFile * file);

// Scale is a fixed-point value with eight fractional bits. Characters
// absent from the font have zero advance. Keep file storage separate from
// the bitmap. Text starts at the canvas position and uses its pen and clip.
word abm_stroke_text_width(const ABitmapStrokeFont * font, const char * text, word scale);
void abm_canvas_stroke_text(const ABitmapCanvas * canvas, const ABitmapStrokeFont * font,
	const char * text, word scale);

#pragma compile("bitmap_stroke_font.c")

#endif
