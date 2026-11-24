#ifndef ATARI_VBXE_CANVAS_H
#define ATARI_VBXE_CANVAS_H

#include "bitmap_canvas.h"
#include "vbxe_display.h"

struct AVbxeCanvas
{
	const AVbxeDisplay * display;
	word width, left, right;
	byte top, bottom;
};

// Bind a bitmap overlay with an initial full-surface clip. Keep display
// and its board unchanged while bound. Failure leaves canvas unchanged.
bool vbxe_canvas_init(AVbxeCanvas * canvas, const AVbxeDisplay * display);

// Clip to a rectangle with exclusive right and bottom edges. A null
// rectangle restores the entire overlay. Empty clips are valid.
void vbxe_canvas_clip(AVbxeCanvas * canvas, const ABitmapRect * rect);

// Solid-color drawing. Out-of-clip pixels are ignored. HIGH mode accepts
// palette indices 0..15. A valid empty draw succeeds. Preserve pitch
// padding and adjacent packed pixels. Serialize with other MEMAC-A users.
bool vbxe_canvas_line(const AVbxeCanvas * canvas, const ABitmapPoint * from, const ABitmapPoint * to, byte color);
bool vbxe_canvas_rect(const AVbxeCanvas * canvas, const ABitmapRect * rect, byte color, bool filled);

// Draw an outline or fill by the even-odd rule, including the outline.
bool vbxe_canvas_polygon(const AVbxeCanvas * canvas, const ABitmapPoint * points, byte count, byte color, bool filled);

// Draw an arc or a filled sector. Angles follow ABitmapArc (binary turns).
// A full sweep draws an ellipse. Sweep zero draws nothing. Only a full
// sweep permits a zero radius, which produces a line or a point.
bool vbxe_canvas_arc(const AVbxeCanvas * canvas, const ABitmapArc * arc, byte color, bool filled);

// Fill and outline the front face, with the edges of its side and optional top.
bool vbxe_canvas_bar_3d(const AVbxeCanvas * canvas, const ABitmapRect * front, word depth,
	byte fill, byte outline, bool top);

// Replace the seed's four-connected region within the active clip. Supply
// the required scratch in CPU RAM outside MEMAC-A, disjoint from all live
// descriptors and state. Returns false for an invalid seed, color or work.
word vbxe_canvas_flood_size(const AVbxeCanvas * canvas);
bool vbxe_canvas_flood_fill(const AVbxeCanvas * canvas, const ABitmapPoint * seed, byte color, byte * work, word size);

#pragma compile("vbxe_canvas.c")
#pragma compile("vbxe_flood.c")

#endif
