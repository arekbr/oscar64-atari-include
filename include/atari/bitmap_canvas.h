#ifndef ATARI_BITMAP_CANVAS_H
#define ATARI_BITMAP_CANVAS_H

#include "bitmap.h"

struct ABitmapCanvas
{
	const ABitmap * bitmap;
	word left, right;
	byte top, bottom;
	int x, y;
	word line_pattern;
	byte line_width, pen_color, brush_color;
	const byte * brush;
};

// Initialize over a bound bitmap. The initial pen and solid brush use color 1.
void abm_canvas_init(ABitmapCanvas * canvas, const ABitmap * bitmap);

// Intersect a rectangle with the bitmap. Null resets the clip to the bitmap.
// Empty clips are valid. The right and bottom edges are exclusive.
void abm_canvas_clip(ABitmapCanvas * canvas, const ABitmapRect * rect);

// The line pattern repeats from bit 15 at the first point, even when clipped.
// A zero bit skips the point. Width is a square stamp size; zero draws nothing.
void abm_canvas_pen(ABitmapCanvas * canvas, byte color, word pattern, byte width);

// Null selects solid fill. Otherwise provide eight rows of eight bits (MSB
// left), aligned to bitmap coordinate (0,0). Zero bits preserve the destination.
void abm_canvas_brush(ABitmapCanvas * canvas, byte color, const byte * pattern);

void abm_canvas_move(ABitmapCanvas * canvas, int x, int y);
void abm_canvas_to(ABitmapCanvas * canvas, int x, int y);
void abm_canvas_line(const ABitmapCanvas * canvas, int x0, int y0, int x1, int y1);
void abm_canvas_rect(const ABitmapCanvas * canvas, int x0, int y0, int x1, int y1);
void abm_canvas_fill(const ABitmapCanvas * canvas, const ABitmapRect * rect);

// Fill and outline the front face. Draw the side and optional top, offset
// by depth pixels right and up. The canvas position is preserved.
void abm_canvas_bar_3d(const ABitmapCanvas * canvas, const ABitmapRect * front, word depth, bool top);

// Clip the outline to the canvas using the current pen. Polygon fill uses
// the even-odd rule and current brush, then draws the outline with the pen.
void abm_canvas_polygon(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count);
void abm_canvas_fill_polygon(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count);

// Apply the canvas clip to the destination with matching source offsets.
// Overlap rules and the color key follow abm_copy_rect() and abm_blit().
bool abm_canvas_copy_rect(const ABitmapCanvas * canvas, int x, int y, const ABitmap * src, const ABitmapRect * rect);
bool abm_canvas_blit(const ABitmapCanvas * canvas, int x, int y, const ABitmapImage * image, byte key);

struct ABitmapArc
{
	int x, y;
	word rx;
	byte ry, start;
	word sweep;
};

// Angles are in 1/256 turns, clockwise from the positive X axis. Sweep
// 0 is empty and 256 is full. Both radii must be nonzero. The arc uses solid
// pen color. The sector includes both radial edges and uses the brush.
void abm_canvas_arc(const ABitmapCanvas * canvas, const ABitmapArc * arc);
void abm_canvas_sector(const ABitmapCanvas * canvas, const ABitmapArc * arc);

struct ABitmapFont
{
	const byte * glyphs;
	const byte * widths;
	byte spacing;
};

// Eight-row ATASCII text. Optional widths in screen-code order specify the
// leftmost 0..8 pixels of each glyph. Null widths give fixed 8-pixel glyphs.
// Spacing is added only between characters. Width saturates at 65535.
word abm_canvas_text_width(const char * text, const ABitmapFont * font);
void abm_canvas_text(const ABitmapCanvas * canvas, const char * text, const ABitmapFont * font);

#pragma compile("bitmap_canvas.c")
#pragma compile("bitmap_arc.c")

#endif
