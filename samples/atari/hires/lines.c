#include <atari/bitmap_canvas.h>
#include <atari/colors.h>
#include <atari/input.h>
#include "../support/bitmap_demo.h"

static ABitmapCanvas canvas;

// LINOP_SET replaces both the set and the clear bits of a line.
static void pattern_line(int x0, int y0, int x1, int y1, byte pattern)
{
	if (pattern)
	{
		abm_canvas_pen(&canvas, 1, 257 * (word)pattern, 1);
		abm_canvas_line(&canvas, x0, y0, x1, y1);
	}
	if (pattern != 0xff)
	{
		abm_canvas_pen(&canvas, 0, 257 * (word)(byte)~pattern, 1);
		abm_canvas_line(&canvas, x0, y0, x1, y1);
	}
}

static void fan(byte pattern)
{
	for (byte i = 0; i < 40; ++i)
	{
		int x = 8 * i, y = 5 * i;
		pattern_line(x, 0, 319, y, pattern);
		pattern_line(319, y, 319 - x, 199, pattern);
		pattern_line(319 - x, 199, 0, 199 - y, pattern);
		pattern_line(0, 199 - y, x, 0, pattern);
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEF, 200))
		return 1;
	os_color[1] = COLOR_BLACK;
	os_color[2] = COLOR_WHITE;
	os_color[4] = COLOR_WHITE;
	abm_canvas_init(&canvas, &screen);

	static const byte patterns[] = {0xff, 0x00, 0xff, 0xaa, 0x88, 0x80, 0x00};
	for (byte i = 0; i < sizeof(patterns); ++i)
		fan(patterns[i]);

	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
