#include <atari/input.h>
#include <atari/pmg_cursor.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#define PMG ((volatile byte *)0x8000)
#define STATUS (*(volatile byte *)0x0600)

static const byte arrow[] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xe0, 0xb0, 0x18};
static const APmgCursorConfig config = {arrow, sizeof(arrow), 1, 2, 2, 50, 34, 195, 210};
static APmgCursor cursor;

static bool plane_matches(byte top, bool shown)
{
	volatile byte * plane = pmg_player(2);
	for (word row = 0; row < 256; row++)
	{
		byte expect = shown && row >= top && row - top < sizeof(arrow) ? arrow[row - top] : 0;
		if (plane[row] != expect)
			return false;
	}
	return pmg_player(1)[62] == 0x81;
}

static bool exercise(void)
{
	if (!pmg_cursor_move(&cursor, -32768, 32767) || cursor.x != 50 || cursor.y != 210 ||
		!plane_matches(0, false))
		return false;
	if (!pmg_cursor_show(&cursor, true) || !plane_matches(208, true) ||
		!pmg_cursor_step(&cursor, JOY_RIGHT | JOY_UP, 5, 7) ||
		cursor.x != 55 || cursor.y != 203 || !plane_matches(201, true))
		return false;
	if (!pmg_cursor_step(&cursor, JOY_LEFT | JOY_RIGHT | JOY_UP | JOY_DOWN, 255, 255) ||
		cursor.x != 55 || cursor.y != 203 || !plane_matches(201, true) ||
		!pmg_cursor_show(&cursor, false) || !plane_matches(0, false))
		return false;
	if (!pmg_cursor_move(&cursor, 97, 66) || !plane_matches(0, false) ||
		!pmg_cursor_show(&cursor, true) || !plane_matches(64, true))
		return false;
	return true;
}

int main(void)
{
	STATUS = 1;
	if (pmg_cursor_init(&cursor, &config))
		return 1;
	pmg_init(PMG, PMG_2LINE);
	if (pmg_plane_rows() != 128 || pmg_cursor_init(&cursor, &config))
		return 1;
	STATUS = 2;
	pmg_init(PMG, PMG_1LINE);
	if (pmg_plane_rows() != 256 || !pmg_cursor_init(&cursor, &config))
		return 2;
	pmg_player(1)[62] = 0x81;
	os_pcolr[2] = 0x0e;
	os_color[1] = os_color[2] = os_color[4] = 0;
	os_gprior = PRIOR_P0123_PF0123;
	gtia.sizep[2] = 0;
	STATUS = 3;
	if (!exercise())
		return 3;
	STATUS = 0xa5;
	for (;;)
	{
		os_wait_frame();
		pmg_cursor_step(&cursor, joy_read(0), 1, 2);
	}
}
