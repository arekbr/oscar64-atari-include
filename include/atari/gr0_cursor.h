#ifndef ATARI_GR0_CURSOR_H
#define ATARI_GR0_CURSOR_H

#include "types.h"

struct AGr0Cursor
{
	word x;
	byte y;
	volatile byte * screen, * cell;
	byte saved;
};

// Initialize a hidden cursor. Coordinates are in the 320x192 pixel grid
// of a standard OS GRAPHICS 0 screen. One 8x8 character is inverted.
void gr0_cursor_init(AGr0Cursor * cursor);

// Clamp to the screen. Motion within the same character is retained.
bool gr0_cursor_move(AGr0Cursor * cursor, int x, int y);

// Show or restore the cell. Call in the foreground. Hide the OS editor
// caret and hide this cursor during screen writes or mode changes.
bool gr0_cursor_show(AGr0Cursor * cursor, bool visible);

#pragma compile("gr0_cursor.c")

#endif
