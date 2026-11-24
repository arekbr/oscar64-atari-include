#include <atari/colors.h>
#include <atari/input.h>
#include "../support/bitmap_demo.h"
#include "mandelbrot.h"

#define WIDTH 320
#define ROWS 200

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEF, ROWS))
		return 1;

	for (byte y = 0; y < ROWS; ++y)
	{
		float imaginary = y * (2.0 / ROWS) - 1.0;
		volatile byte * row = abm_row(&screen, y);
		for (word x = 0; x < WIDTH; x += 8)
		{
			byte bits = 0;
			for (byte bit = 0; bit < 8; ++bit)
			{
				float real = (x + bit) * (3.5 / WIDTH) - 2.5;
				bits = (bits << 1) | (escape(real, imaginary) < ITERATIONS);
			}
			*row++ = bits;
		}
	}

	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
