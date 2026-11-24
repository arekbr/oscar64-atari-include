#ifndef ATARI_SAMPLE_MANDELBROT_FIXED_H
#define ATARI_SAMPLE_MANDELBROT_FIXED_H

#include <fixmath.h>
#include <atari/types.h>

#define SCALE 4096
#define LIMIT 32

// Coordinates use signed 4.12 fixed point and the squared radius uses 8.24.
static byte escape_fixed(int real, int imaginary)
{
	int x = 0, y = 0;
	for (byte step = 0; step < LIMIT; ++step)
	{
		unsigned long xx = lsqr4f12s(x), yy = lsqr4f12s(y);
		if (xx + yy >= 4UL * SCALE * SCALE)
			return step;
		int next_x = (int)(xx >> 12) - (int)(yy >> 12) + real;
		y = 2 * lmul4f12s(x, y) + imaginary;
		x = next_x;
	}
	return LIMIT;
}

#endif
