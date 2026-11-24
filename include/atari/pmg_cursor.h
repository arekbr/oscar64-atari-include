#ifndef ATARI_PMG_CURSOR_H
#define ATARI_PMG_CURSOR_H

#include "pmg.h"

struct APmgCursorConfig
{
	const byte * scanlines;
	byte height, hot_x, hot_y, player;
	byte left, top, right, bottom;
};

struct APmgCursor
{
	APmgCursorConfig config;
	byte x, y;
	bool visible;
};

// Reserve one player after pmg_init(). The caller sets its normal size,
// color and priority and keeps its plane exclusively available. Supply
// a stable bitmap outside PMG RAM. Coordinates are GTIA color clocks
// horizontally and PMG memory rows vertically. The box has inclusive
// limits for the hot spot. The cursor starts hidden at the top left.
bool pmg_cursor_init(APmgCursor * cursor, const APmgCursorConfig * config);

// Clamp the hot spot to the box, retaining the old position if drawing fails.
bool pmg_cursor_move(APmgCursor * cursor, int x, int y);
bool pmg_cursor_show(APmgCursor * cursor, bool visible);

// Apply JOY_* bits (for example from joy_read()). Opposite directions
// cancel. Steps are in the same units as the cursor coordinates.
bool pmg_cursor_step(APmgCursor * cursor, byte directions, byte step_x, byte step_y);

#pragma compile("pmg_cursor.c")

#endif
