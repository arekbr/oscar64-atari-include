#ifndef ATARI_VBXE_TERMINAL_H
#define ATARI_VBXE_TERMINAL_H

#include "vbxe_display.h"

struct AVbxeTerminal
{
	const AVbxeDisplay * display;
	const byte * glyphs;
	byte columns, rows, column, row, top, bottom;
	byte foreground, background, saved_column, saved_row, saved_foreground, saved_background;
	bool inverse, saved_inverse, wrap, pending, cursor, shown;
	byte state, argument, params[8];
	bool private_csi;
	AVbxeTextCell saved_cell, work[84];
	AVbxeTextCell * scrollback;
	bool scrollback_ready;
};

// Use a stable hardware text display and an optional 128-entry ASCII to
// glyph table. Initialization leaves VRAM and the display untouched.
// Call palette and clear to initialize the visible terminal.
bool vbxe_terminal_init(AVbxeTerminal * terminal, const AVbxeDisplay * display, const byte * glyphs);
bool vbxe_terminal_palette(const AVbxeTerminal * terminal);
bool vbxe_terminal_clear(AVbxeTerminal * terminal);
// Scroll inclusive text rows. Positive amounts move the contents up.
bool vbxe_terminal_scroll(AVbxeTerminal * terminal, byte first, byte last, int rows);
// Provide a separate CPU buffer of at least terminal->columns cells outside
// MEMAC-A. Passing null disables capture. Each nonzero scroll keeps the
// first outgoing row (top for up, bottom for down), including attributes
// and the cell under the cursor. A new capture replaces the previous row.
bool vbxe_terminal_scrollback_bind(AVbxeTerminal * terminal, AVbxeTextCell * cells, byte capacity);
// Consume the ready flag. The row stays in the caller's buffer.
bool vbxe_terminal_scrollback_take(AVbxeTerminal * terminal);
bool vbxe_terminal_cursor(AVbxeTerminal * terminal, bool visible);
// Feed ASCII and ANSI control bytes. An incomplete sequence can continue
// in the next call. Use the opaque text display for colored backgrounds.
bool vbxe_terminal_write(AVbxeTerminal * terminal, const char * data, word count);

#pragma compile("vbxe_terminal.c")

#endif
