#include <atari/colors.h>
#include <atari/input.h>
#include <string.h>
#include "../support/bitmap_demo.h"
#include "../support/stipple.h"

static byte brush_data[8 * 64];
static ABitmap brush;
static const ABitmapPoint shape[] = {
	{0,64},{32,0},{32,20},{24,36},{40,36},{32,20},
	{32,0},{64,64},{52,64},{42,44},{22,44},{12,64}
};
static const byte pattern[] = {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0};

static const char * const operation[] = {
	"SET", "RESET", "NOT", "XOR", "OR", "AND", "NAND", "COPY", "NCOPY", "PATTERN", "MASKPAT"
};

static byte apply(byte op, byte src, byte dst, byte pat)
{
	switch (op)
	{
	case 0: return 0xff;
	case 1: return 0;
	case 2: return ~dst;
	case 3: return src ^ dst;
	case 4: return src | dst;
	case 5: return src & dst;
	case 6: return dst & ~src;
	case 7: return src;
	case 8: return ~src;
	case 9: return pat;
	default: return pat & src;
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEF, 200))
		return 1;
	// The offscreen mask contains eight bytes per row.
	brush = (ABitmap){brush_data, 64, 64, 8, 1};
	os_color[4] = COLOR_WHITE;
	abm_clear(&brush, 0);
	abm_fill_polygon(&brush, shape, sizeof(shape) / sizeof(shape[0]), 1);
	for (byte y = 0; y < 200; ++y)
	{
		volatile byte * row = abm_row(&screen, y);
		for (byte x = 0; x < 40; ++x)
			row[x] = gray[2][y & 7];
	}

	for (byte i = 0; i < sizeof(operation) / sizeof(operation[0]); ++i)
	{
		byte col = ((i + 1) & 3) * 10 + 1, row = 66 * ((i + 1) >> 2);
		for (byte y = 0; y < 64; ++y)
		{
			volatile byte * dest = abm_row(&screen, row + 1 + y) + col;
			for (byte x = 0; x < 8; ++x)
				dest[x] = apply(i, brush_data[8 * y + x], dest[x], pattern[(row + 1 + y) & 7]);
		}
		abm_fill_rect(&screen, 8 * col, row + 20, 8 * strlen(operation[i]), 8, 0);
		abm_text(&screen, 8 * col, row + 20, operation[i], (const byte *)0xe000, 1);
	}
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
