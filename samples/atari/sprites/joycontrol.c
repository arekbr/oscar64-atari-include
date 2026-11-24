#include <atari/input.h>
#include "../support/bear.h"

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte colors[4];
	for (byte i = 0; i < 4; ++i) colors[i] = os_pcolr[i];
	bear_init();
	byte y = 100;
	int sx = 160;
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		byte stick = joy_read(0);
		sx += ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
		y += ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);
		bear_move(0, sx & 511, y, false);
	}
	pmg_off();
	for (byte i = 0; i < 4; ++i) os_pcolr[i] = colors[i];
	bitmap_demo_restore(&saved);
	return 0;
}
