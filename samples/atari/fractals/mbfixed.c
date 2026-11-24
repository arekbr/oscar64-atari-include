#include <atari/colors.h>
#include <atari/input.h>
#include "../support/bitmap_demo.h"
#include <fixmath.h>

static const byte colors[] = {
	0xff, 0xff,
	0xee, 0xbb,
	0xaa, 0xaa,
	0x88, 0x22,

	0x44, 0x11,
	0x55, 0x55,
	0xdd, 0x77,
	0x33, 0xcc
};

static __noinline void draw_row(byte y)
{
	volatile byte * upper = abm_row(&screen, y * 2);
	volatile byte * lower = abm_row(&screen, y * 2 + 1);
	int imaginary = lmul8f8s(y, (int)((2.4 / 100.0) * 4096 * 256)) - (int)(1.2 * 4096);
	for (byte x = 0; x < 160; ++x)
	{
		int real = lmul8f8s(x, (int)((3.5 / 160.0) * 4096 * 256)) - (int)(2.5 * 4096);
		int rx = 0, iy = 0;
		byte steps;
		for (steps = 0; steps < 32; ++steps)
		{
			unsigned long xx = lsqr4f12s(rx), yy = lsqr4f12s(iy);
			if (xx + yy >= 0x04000000UL)
				break;
			int next = (int)(xx >> 12) - (int)(yy >> 12) + real;
			iy = 2 * lmul4f12s(rx, iy) + imaginary;
			rx = next;
		}
		if (steps < 32)
		{
			byte mask = 0xc0 >> (2 * (x & 3));
			byte color = (steps & 7) * 2;
			upper[x >> 2] |= colors[color] & mask;
			lower[x >> 2] |= colors[color + 1] & mask;
		}
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEE, 200))
		return 1;
	os_color[0] = COLOR_RED;
	os_color[1] = COLOR_YELLOW;
	os_color[2] = COLOR_CYAN;

	for (byte y = 0; y < 100; ++y)
		draw_row(y);

	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
