#include <atari/colors.h>
#include <atari/input.h>
#include <stdlib.h>
#include "../support/bitmap_demo.h"

#define WIDTH 160
#define ROWS 200
static byte work[(WIDTH / 8) * ROWS];

static void circles(byte color)
{
	for (byte i = 0; i < 20; ++i)
	{
		int x = rand() % 320;
		byte y = rand() % ROWS;
		byte r = 5 + rand() % 40;
		abm_fill_ellipse(&screen, x >> 1, y, (r + 1) >> 1, r, color);
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEE, ROWS))
		return 1;
	os_color[0] = COLOR_BLUE;
	os_color[1] = COLOR_YELLOW;
	os_color[2] = COLOR_RED;
	while (!(cons_keys() & CONSOL_START))
	{
		circles(1);
		circles(0);
		abm_flood_fill(&screen, 105, 100, 2, work, sizeof(work));
		abm_flood_fill(&screen, 30, 140, 3, work, sizeof(work));
	}
	bitmap_demo_restore(&saved);
	return 0;
}
