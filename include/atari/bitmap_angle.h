#ifndef ATARI_BITMAP_ANGLE_H
#define ATARI_BITMAP_ANGLE_H

// Sine of 0..64 binary angle units, rounded to a scale of 256.
static const word sine_quarter[65] = {
	0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68,
	74, 80, 86, 92, 98, 104, 109, 115, 121, 126, 132, 137,
	142, 147, 152, 157, 162, 167, 172, 177, 181, 185, 190, 194,
	198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234,
	237, 239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254,
	255, 255, 256, 256, 256
};

static int bitmap_sine(byte angle)
{
	byte index = angle & 63;
	if (angle & 64)
		index = 64 - index;
	int value = sine_quarter[index];
	return angle & 128 ? -value : value;
}

#endif
