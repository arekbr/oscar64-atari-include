#define MAP_TILES 16
#define TOP_ROWS 0
#define MODE DL_MODE4
#include "grid.h"

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte chbase = os_chbas;
	make_map();
	grid_display();
	word x = 0, y = 0;
	grid_position(x, y);
	dl_set(list);

	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		byte stick = joy_read(0);
		if ((stick & JOY_RIGHT) && x < 24 * 8)
			x += 2;
		else if ((stick & JOY_LEFT) && x)
			x -= 2;
		else if ((stick & JOY_DOWN) && y < 39 * 8)
			y += 2;
		else if ((stick & JOY_UP) && y)
			y -= 2;
		grid_position(x, y);
	}

	os_chbas = chbase;
	bitmap_demo_restore(&saved);
	return 0;
}
