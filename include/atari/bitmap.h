#ifndef ATARI_BITMAP_H
#define ATARI_BITMAP_H

#include "antic.h"

// Four-bit pixels on an ANTIC F display list. Select GTIA mode 9, 10 or 11 in PRIOR.
#define ABM_GTIA	0x10

struct ABitmapMode
{
	word	width;		// Logical pixels per row.
	byte	row_bytes;	// Bytes fetched per row.
	byte	bits;		// Bits per pixel.
	byte	scanlines;	// Scan lines per bitmap row.
	byte	antic_mode;	// Display-list instruction without flags.
};

struct ABitmap
{
	volatile byte *	data;
	word			width;
	byte			height;
	byte			pitch;
	byte			bits;
};

// Layout for ANTIC modes 8 through F and ABM_GTIA. Unknown modes return null.
const ABitmapMode * abm_mode_info(byte mode);

// Describe the layout at DMACTL_PF_NARROW, DMACTL_PF_NORMAL or DMACTL_PF_WIDE.
// Invalid input leaves info unchanged.
bool abm_mode_info_width(byte mode, byte playfield, ABitmapMode * info);

// Bind the view to a caller-owned buffer for a normal-width bitmap.
// pitch is the distance between row starts in bytes. Failed binding leaves
// the view unchanged. The caller sets up the display list and video memory.
bool abm_init(ABitmap * bm, volatile byte * data, word size, byte pitch, byte height, byte mode);

// Bind a bitmap for the specified DMACTL playfield width.
bool abm_init_width(ABitmap * bm, volatile byte * data, word size, byte pitch, byte height, byte mode, byte playfield);

// Use a bound view. Coordinates start at the upper left.
// Drawing uses the low color bits. An out-of-bounds read returns zero.
byte abm_get(const ABitmap * bm, word x, byte y);

// Address of a packed scanline, or null for a row outside the bitmap.
volatile byte * abm_row(const ABitmap * bm, byte y);

// Write one pixel. Out-of-bounds coordinates leave the bitmap unchanged.
void abm_put(const ABitmap * bm, word x, byte y, byte color);

// Fill the visible pixels. Bytes between rows are left alone.
void abm_clear(const ABitmap * bm, byte color);

// Draw a line including both endpoints. Both endpoints must be in bounds.
void abm_line(const ABitmap * bm, word x0, byte y0, word x1, byte y1, byte color);

// Fill width by height pixels from x, y, clipped at the right and bottom edges.
void abm_fill_rect(const ABitmap * bm, word x, byte y, word width, byte height, byte color);

// Draw a clipped ellipse. The outline is the four-neighbor boundary of the
// filled shape. A zero radius produces a line or point. Radii greater than
// the corresponding bitmap dimension leave the bitmap unchanged.
void abm_ellipse(const ABitmap * bm, int cx, int cy, word rx, byte ry, byte color);
void abm_fill_ellipse(const ABitmap * bm, int cx, int cy, word rx, byte ry, byte color);

// Return the number of scratch bytes required for flood fill.
word abm_flood_size(const ABitmap * bm);

// Replace the seed color in its four-connected region. The scratch buffer
// must hold abm_flood_size() bytes and be separate from the bitmap and view.
// An invalid seed or scratch buffer returns false without changing either.
// If the seed already has the masked color, return true without using work.
bool abm_flood_fill(const ABitmap * bm, word x, byte y, byte color, byte * work, word size);

// Draw a line of NUL-terminated ATASCII text with a transparent background.
// font holds 128 eight-byte glyphs in screen-code order, most significant bit
// at the left. Inverse ATASCII inverts the glyph bits. Both axes are clipped.
void abm_text(const ABitmap * bm, int x, int y, const char * text, const byte * font, byte color);

struct ABitmapRect
{
	int x, y;
	word width;
	byte height;
};

struct ABitmapImage
{
	const volatile byte * data;
	word size, width;
	byte height, pitch, bits;
};

#define ABM_OPAQUE 0xff

// Copy a rectangle between bound bitmaps of the same pixel depth. Clip to
// both views. A view can copy onto itself, even if the rectangles overlap.
// Views with overlapping storage must describe the same layout.
bool abm_copy_rect(const ABitmap * dst, int x, int y, const ABitmap * src, const ABitmapRect * rect);

// Draw a packed image, most significant pixel first in each byte. The image
// and destination must have the same depth and separate storage. Set key to
// ABM_OPAQUE to copy all pixels, or to a color index to skip that color.
// Both calls return true for an empty clipped rectangle and false on invalid input.
bool abm_blit(const ABitmap * dst, int x, int y, const ABitmapImage * image, byte key);

struct ABitmapPoint
{
	int x, y;
};

// Draw the closed outline through count vertices. A single point is drawn
// once and two vertices form a line. Segments are clipped to the bitmap.
void abm_polygon(const ABitmap * bm, const ABitmapPoint * points, byte count, byte color);

// Fill by the even-odd rule, including the rasterized outline. Concave and
// self-intersecting polygons are supported. No caller scratch is required.
void abm_fill_polygon(const ABitmap * bm, const ABitmapPoint * points, byte count, byte color);

// Build a display list for the bound layout in a separate caller-owned buffer.
// top is the blank space above the bitmap. The total must fit 240 scan lines.
// A row must fit within one 4 KB page. The list must fit within one 1 KB page.
// Return the number of bytes written, or zero on failure without modifying list.
// Select the matching playfield width in DMACTL and install the list with dl_set().
word abm_display_list(volatile byte * list, word capacity, const ABitmap * bm, byte mode, byte top);

// Validate the same layout and return the required bytes without writing the list.
word abm_display_list_size(const volatile byte * list, word capacity, const ABitmap * bm, byte mode, byte top);

#pragma compile("bitmap.c")
#pragma compile("bitmap_draw.c")
#pragma compile("bitmap_blit.c")
#pragma compile("bitmap_polygon.c")

#endif
