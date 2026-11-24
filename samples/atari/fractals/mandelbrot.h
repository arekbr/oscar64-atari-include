#ifndef ATARI_SAMPLE_MANDELBROT_H
#define ATARI_SAMPLE_MANDELBROT_H

#include <atari/types.h>

#define ITERATIONS 32

static byte escape(float real, float imaginary)
{
	float x = 0, y = 0;
	for (byte i = 0; i < ITERATIONS; ++i)
	{
		float xx = x * x, yy = y * y;
		if (xx + yy > 4.0)
			return i;
		y = 2.0 * x * y + imaginary;
		x = xx - yy + real;
	}
	return ITERATIONS;
}

#endif
