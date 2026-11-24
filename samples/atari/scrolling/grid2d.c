#define MAP_TILES 32
#define TOP_ROWS 5
#define MODE DL_MODE2
#include "grid.h"

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte chbase = os_chbas;
	make_map();
	grid_display();

	int px = 40 * 32, py = 40 * 32, dx = 0, dy = 0, ax = 0, ay = 0;
	grid_position(px >> 5, py >> 5);
	dl_set(list);

	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		grid_position(px >> 5, py >> 5);

		dx += ax;
		dy += ay;
		if ((rand() & 63) == 0)
		{
			ax = (rand() & 63) - 32;
			ay = (rand() & 63) - 32;
		}
		dx -= (dx + 8) >> 4;
		dy -= (dy + 8) >> 4;

		px += dx;
		if (px < 0 || px > 22 * 8 * 4 * 32)
		{
			dx = -dx;
			px += dx;
		}
		py += dy;
		if (py < 0 || py > 26 * 8 * 4 * 32)
		{
			dy = -dy;
			py += dy;
		}
	}

	os_chbas = chbase;
	bitmap_demo_restore(&saved);
	return 0;
}
