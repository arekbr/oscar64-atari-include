#include "color_cells.h"

// Lookup table for squares from 0..255
__striped unsigned sqb[256];

// Shift byte right and left by 4
char shlb4[256], shrb4[256];

#pragma align(sqb, 256);

// Square an unsigned int into an unsigned long
inline unsigned long ssquare(unsigned x)
{
	// Split into low byte and highbyte, so we have x = a + 0x100 * b

	unsigned	a = x & 0xff;
	unsigned	b = x >> 8;

	// So now we calculate (a + 0x100 * b)²
	// Result will be a² + 0x100 * 2 * a * b + 0x10000 * b²
	// with 2 * a * b == (a + b)² - a² - b²

	// We can cover all cases with the square table except if a + b >= 0x100
	// in this case we have abp := a + b - 0x100
	// (abp + 0x100)² == abp² + 2 * 0x100 * abp + 0x10000

	// Get squares of the bytes and the sum of the bytes
	unsigned	a2 = sqb[a], b2 = sqb[b];
	unsigned	apb = a + b;

	// First term
	// a² + 0x10000 * b²
	unsigned long sum = a2 + ((unsigned long)b2 << 16);

	// Check if a + b >= 0x100
	if (apb & 0xff00)
	{
		apb &= 0xff;
		sum += 0x1000000UL;
		sum += (unsigned long)apb << 17;
		sum += (unsigned long)sqb[apb] << 8;
	}
	else
	{
		apb &= 0xff;
		sum += (unsigned long)sqb[apb] << 8;
	}

	// Now we have a² + 0x1000 * b² + (a + b)²

	sum -= (unsigned long)a2 << 8;
	sum -= (unsigned long)b2 << 8;

	// And finally the complete result
	return sum;
}

// Signed square of x
inline long sq(int x)
{
	if (x < 0)
		x = -x;
	return ssquare(x);
}

// Colors to fill in the different levels
static const char colors[32] = {
	6,
	14,
	1,
	13,
	5,
	7,
	8,
	2,
	4,

	6,
	6,
	14,
	14,
	1,
	1,
	13,
	13,
	5,
	5,
	7,
	7,
	8,
	8,
	2,
	2,
	4,
	4,

	15,
	15,
	12,
	12,
	11,
};

inline int shr12(long l)
{
	char b3 = (l >> 24) & 0xff;
	char b2 = (l >> 16) & 0xff;
	char b1 = (l >>  8) & 0xff;
	char a0 = shrb4[b1] | shlb4[b2];
	char a1 = shrb4[b2] | shlb4[b3];
	return a0 | (a1 << 8);
}

// Return color for a given coordinate in the complex plane using
// 4.12-bit fixed numbers using m'=m²+b

inline char fcolor(int xz, int yz)
{
	// Start value for iteration is the offset value itself

	int		x = xz, y = yz;

	// Iterate up to 32 steps

	for(int i=0; i<32; i++)
	{
		// Build squares of real and imaginary component
		long xx = sq(x), yy = sq(y), xxyy = sq(x + y);

		long xxpyy = xx + yy;

		// Use squares to check for exit condition of sure
		// to progress towards infinity
		if (xxpyy >= 4L * 4096 * 4096) return colors[i];

		// Next iteration values using complex arithmetic
		// Mx' = Mx² - My² + Bx
		// My' = 2 * Mx * My + By = (Mx + My)² - Mx² - My² + By
		x = shr12(xx - yy + 2048) + xz;
		y = shr12(xxyy - xxpyy + 2048) + yz;
	}

	// More than maximum number of iterations, so assume progress
	// towards zero

	return 0;
}

// Fill a row with color
void fill_row(char py, int cix, int yz, int cis)
{
	int	xz = cix;
	for(int px=0; px<40; px++)
	{
		cells[py][px] = pixels[fcolor(xz, yz)];
		xz += cis;
	}
}

// Fill a column with color
void fill_column(char px, int xz, int ciy, int cis)
{
	int	yz = ciy;
	for(int py=0; py<25; py++)
	{
		cells[py][px] = pixels[fcolor(xz, yz)];
		yz += cis;
	}
}

// Fill the complete image
void fill_image(int cix, int ciy, int cis)
{
	int		yz = ciy;
	for(int py=0; py<25; py++)
	{
		fill_row(py, cix, yz, cis);
		yz += cis;
	}
}

// Scroll screen to the left
void scroll_left(void)
{
	for(char x=0; x<39; x++)
	{
		#pragma unroll(full)
		for(char y=0; y<25; y++)
		{
			cells[y][x] = cells[y][x + 1];
		}
	}
}

// Scroll screen to the right
void scroll_right(void)
{
	for(signed char x=38; x>=0; x--)
	{
		#pragma unroll(full)
		for(char y=0; y<25; y++)
		{
			cells[y][x + 1] = cells[y][x];
		}
	}
}

// Scroll screen up
void scroll_up(void)
{
	for(char x=0; x<40; x++)
	{
		#pragma unroll(full)
		for(char y=0; y<24; y++)
		{
			cells[y][x] = cells[(y + 1)][x];
		}
	}
}

// Scroll screen down
void scroll_down(void)
{
	for(char x=0; x<40; x++)
	{
		#pragma unroll(full)
		for(char y=0; y<24; y++)
		{
			cells[(24 - y)][x] = cells[(23 - y)][x];
		}
	}
}

int main(void)
{
	// Initialize square table
	for(unsigned i=0; i<256; i++)
	{
		sqb[i] = i * i;
		shlb4[i] = i << 4;
		shrb4[i] = i >> 4;
	}

	ColorCellsState saved;
	show_cells(&saved);

	// Start coordinates in float
	float cx = -0.4;
	float cy = 0.0;
	float cw = 3.2;

	// Convert to top, left and step in 4.12 fixed point
	int		cix = (int)((cx - 0.5 * cw) * 4096);
	int		ciy = (int)((cy - 12.0 * cw / 40.0) * 4096);
	int		cis = (int)(cw / 40.0 * 4096);

	// Initial image
	fill_image(cix, ciy, cis);

	for(;;)
	{
		// Wait for keypress
		byte ch = key_getc();
		if (ch >= 'a' && ch <= 'z')
			ch -= 'a' - 'A';

		switch (ch)
		{
		case ATASCII_ESC:
			close_cells(&saved);
			return 0;
		case 'S':
			ciy += cis;
			scroll_up();
			fill_row(24, cix, ciy + 24 * cis, cis);
			break;
		case 'W':
			ciy -= cis;
			scroll_down();
			fill_row(0, cix, ciy, cis);
			break;
		case 'A':
			cix -= cis;
			scroll_right();
			fill_column(0, cix, ciy, cis);
			break;
		case 'D':
			cix += cis;
			scroll_left();
			fill_column(39, cix + 39 * cis, ciy, cis);
			break;
		case '+':
			cix += 20 * cis;
			ciy += 12 * cis;
			cis = cis * 2 / 3;
			cix -= 20 * cis;
			ciy -= 12 * cis;
			fill_image(cix, ciy, cis);
			break;
		case '-':
			cix += 20 * cis;
			ciy += 12 * cis;
			cis = cis * 3 / 2;
			cix -= 20 * cis;
			ciy -= 12 * cis;
			fill_image(cix, ciy, cis);
			break;
		}
	}

	return 0;
}
