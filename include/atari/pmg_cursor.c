#include "pmg_cursor.h"
#include "input.h"

static bool draw(const APmgCursorConfig * config, byte x, byte y, bool visible)
{
	PMGShape shape = {config->scanlines, visible ? (word)config->height : 0,
		x - config->hot_x, y - config->hot_y};
	return pmg_move_player(config->player, &shape);
}

bool pmg_cursor_init(APmgCursor * cursor, const APmgCursorConfig * config)
{
	if (!cursor || !config || !config->scanlines || !config->height ||
		config->hot_x >= 8 || config->hot_y >= config->height ||
		config->left < config->hot_x || config->top < config->hot_y ||
		config->left > config->right || config->top > config->bottom ||
		config->bottom - config->hot_y >= pmg_plane_rows())
		return false;
	if (!draw(config, config->left, config->top, false))
		return false;
	cursor->config = *config;
	cursor->x = config->left;
	cursor->y = config->top;
	cursor->visible = false;
	return true;
}

static byte clamp(int value, byte low, byte high)
{
	return value < low ? low : value > high ? high : value;
}

bool pmg_cursor_move(APmgCursor * cursor, int x, int y)
{
	if (!cursor)
		return false;
	const APmgCursorConfig * config = &cursor->config;
	byte next_x = clamp(x, config->left, config->right);
	byte next_y = clamp(y, config->top, config->bottom);
	if (next_x == cursor->x && next_y == cursor->y)
		return true;
	if (cursor->visible && !draw(config, next_x, next_y, true))
		return false;
	cursor->x = next_x;
	cursor->y = next_y;
	return true;
}

bool pmg_cursor_show(APmgCursor * cursor, bool visible)
{
	if (!cursor)
		return false;
	if (cursor->visible == visible)
		return true;
	if (!draw(&cursor->config, cursor->x, cursor->y, visible))
		return false;
	cursor->visible = visible;
	return true;
}

bool pmg_cursor_step(APmgCursor * cursor, byte directions, byte step_x, byte step_y)
{
	if (!cursor)
		return false;
	int dx = (directions & JOY_RIGHT ? step_x : 0) - (directions & JOY_LEFT ? step_x : 0);
	int dy = (directions & JOY_DOWN ? step_y : 0) - (directions & JOY_UP ? step_y : 0);
	return pmg_cursor_move(cursor, cursor->x + dx, cursor->y + dy);
}
