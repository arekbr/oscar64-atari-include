#ifndef ATARI_VBXE_DISPLAY_H
#define ATARI_VBXE_DISPLAY_H

#include "vbxe.h"

#define VBXE_DISPLAY_LOW 0
#define VBXE_DISPLAY_STANDARD 1
#define VBXE_DISPLAY_HIGH 2
#define VBXE_DISPLAY_TEXT 3
#define VBXE_DISPLAY_TEXT_80X32 4
#define VBXE_DISPLAY_TEXT_80X30 5

#define VBXE_DISPLAY_NARROW 0
#define VBXE_DISPLAY_NORMAL 1
#define VBXE_DISPLAY_WIDE 2
#define VBXE_DISPLAY_XDL_SIZE 16
#define VBXE_DISPLAY_MAP_XDL_SIZE 25
#define VBXE_DISPLAY_80X32_XDL_SIZE (VBXE_DISPLAY_XDL_SIZE + 31 * 8)
#define VBXE_DISPLAY_80X32_MAP_XDL_SIZE (VBXE_DISPLAY_MAP_XDL_SIZE + 31 * 8)
#define VBXE_DISPLAY_80X30_XDL_SIZE (VBXE_DISPLAY_XDL_SIZE + 29 * 8)
#define VBXE_DISPLAY_80X30_MAP_XDL_SIZE (VBXE_DISPLAY_MAP_XDL_SIZE + 29 * 8)

struct AVbxeDisplay
{
	const AVbxe * board;
	unsigned long list, pixels, charset;
	word pitch;
	byte mode, width, height, top;
	byte palette, playfield_palette, priority, control;
};

// Width in pixels for graphics, or cells for text.
word vbxe_display_width(const AVbxeDisplay * display);
// Number of text rows in a valid hardware text surface, zero otherwise.
byte vbxe_display_rows(const AVbxeDisplay * display);

// Height and top are measured in scanlines. Supply a disjoint XDL allocation
// and, for text, a disjoint 2 KiB character set aligned to 2 KiB in VRAM.
// TEXT_80X30 and TEXT_80X32 use the top seven lines of each glyph.
// Use NORMAL width, a height of 210 or 224, and the corresponding XDL size.
// The control field uses the VBXE_* color and transparency flags.
bool vbxe_display_show(const AVbxeDisplay * display);

struct AVbxeColorMap
{
	unsigned long data;
	word pitch;
	byte cell_width, cell_height, scroll_x, scroll_y;
};

// Add a VBXE attribute map to the overlay. Cells are four FX bytes.
// Width is measured in half color clocks. Height is measured in scanlines.
// Use a disjoint XDL with VBXE_DISPLAY_MAP_XDL_SIZE bytes, or the matching
// map XDL size for condensed text. Scroll amounts range from zero through
// one less than the corresponding cell dimension.
bool vbxe_display_show_map(const AVbxeDisplay * display, const AVbxeColorMap * map);
bool vbxe_display_map_priority(const AVbxe * board, byte entry, byte priority);

struct AVbxeDisplayCopy
{
	unsigned long command;
	word source_x, dest_x, width;
	byte source_y, dest_y, height;
	bool transparent;
};

// Start an overlapping rectangle copy in one surface. Coordinates use
// pixels for bitmaps and cells for text. Packed high-resolution copies
// require even X coordinates and width. Zero transparency is available
// for 8-bit graphics. Supply a separate BCB and wait for completion
// before reusing the command or accessing the affected VRAM.
bool vbxe_display_copy(const AVbxeDisplay * display, const AVbxeDisplayCopy * copy);


// Coordinates are relative to the overlay. A filled rectangle uses
// inclusive coordinates. Graphics calls preserve unused nibbles and pitch
// padding. Return false for invalid coordinates, mode or color.
bool vbxe_display_get(const AVbxeDisplay * display, word x, byte y, byte * color);
bool vbxe_display_put(const AVbxeDisplay * display, word x, byte y, byte color);
bool vbxe_display_rect(const AVbxeDisplay * display, word x0, byte y0, word x1, byte y1, byte color);
bool vbxe_display_line(const AVbxeDisplay * display, word x0, byte y0, word x1, byte y1, byte color);

#define VBXE_INK_OPAQUE 1
#define VBXE_INK_INVERSE 2
#define VBXE_INK_UNDERLINE 4
#define VBXE_INK_CONDENSED 8

struct ABitmapFont;

struct AVbxeBitmapText
{
	const ABitmapFont * font;
	int x, y;
	byte foreground, background, flags;
};

// Draw clipped ATASCII text in a bitmap overlay. Use the same screen-code
// font and optional proportional widths as the Atari bitmap canvas.
// OPAQUE paints glyph backgrounds. CONDENSED uses the top seven glyph lines.
bool vbxe_display_text(const AVbxeDisplay * display, const AVbxeBitmapText * ink, const char * text);

struct AVbxeTextCell
{
	byte glyph, attribute;
};

// Read or write cells within a single hardware text row.
bool vbxe_display_read_text(const AVbxeDisplay * display, byte x, byte y, AVbxeTextCell * cells, byte count);
bool vbxe_display_write_text(const AVbxeDisplay * display, byte x, byte y, const AVbxeTextCell * cells, byte count);

#pragma compile("vbxe_display.c")

#endif
