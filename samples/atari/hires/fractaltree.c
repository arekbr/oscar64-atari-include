#include <atari/colors.h>
#include <atari/input.h>
#include <math.h>
#include "../support/bitmap_demo.h"

static void branch(float x, float y, float angle, float length)
{
	if (length < 6.0)
		return;
	float tip_x = x + cos(angle) * length;
	float tip_y = y + sin(angle) * length;
	abm_line(&screen, (word)x, (byte)y, (word)tip_x, (byte)tip_y, 1);
	branch(tip_x, tip_y, angle + 0.3, length * 0.9);
	branch(tip_x, tip_y, angle - 0.4, length * 0.8);
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
	branch(140, 199, PI * 1.5, 32);
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
