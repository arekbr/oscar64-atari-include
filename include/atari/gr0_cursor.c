#include "gr0_cursor.h"
#include "os.h"

static __noinline volatile byte * gr0_screen(void)
{
	if (os_dindex != 0)
		return nullptr;
	const volatile byte * savmsc = (const volatile byte *)0x0058;
	word low = savmsc[0], high = savmsc[1];
	word address = low | high << 8;
	if (address < 0x0600u || address > 0xc000u - 960)
		return nullptr;
	return (volatile byte *)address;
}

void gr0_cursor_init(AGr0Cursor * cursor)
{
	cursor->x = 0;
	cursor->y = 0;
	cursor->screen = nullptr;
	cursor->cell = nullptr;
	cursor->saved = 0;
}

static void restore_cell(AGr0Cursor * cursor, volatile byte * screen)
{
	volatile byte * cell = cursor->cell;
	if (cell && cursor->screen == screen && *cell == (cursor->saved ^ 0x80))
		*cell = cursor->saved;
	cursor->cell = nullptr;
}

bool gr0_cursor_show(AGr0Cursor * cursor, bool visible)
{
	if (!cursor)
		return false;
	volatile byte * screen = gr0_screen();
	if (!visible || cursor->screen != screen)
		restore_cell(cursor, screen);
	if (!visible)
		return true;
	if (!screen)
		return false;
	if (!cursor->cell)
	{
		word offset = (word)(cursor->y >> 3) * 40u + (cursor->x >> 3);
		cursor->screen = screen;
		volatile byte * cell = screen + offset;
		cursor->saved = *cell;
		*cell = cursor->saved ^ 0x80;
		cursor->cell = cell;
	}
	return true;
}

bool gr0_cursor_move(AGr0Cursor * cursor, int x, int y)
{
	if (!cursor)
		return false;
	word next_x = x < 0 ? 0 : x > 319 ? 319 : x;
	byte next_y = y < 0 ? 0 : y > 191 ? 191 : y;
	bool visible = cursor->cell != nullptr;
	bool redraw = visible && ((cursor->x >> 3) != (next_x >> 3) ||
		(cursor->y >> 3) != (next_y >> 3) || cursor->screen != gr0_screen());
	if (redraw)
		gr0_cursor_show(cursor, false);
	cursor->x = next_x;
	cursor->y = next_y;
	return !redraw || gr0_cursor_show(cursor, true);
}
