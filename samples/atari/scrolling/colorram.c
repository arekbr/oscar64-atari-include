#include <atari/colors.h>
#include <stdlib.h>
#include "columns.h"

static byte font[1024];
#pragma align(font, 1024)

// Two inks per shade, alternating on successive scan lines. ANTIC 4
// uses four playfield registers, so the source's intermediate hues mix inks.
static const byte mix[16][2] = {
	{0,0}, {1,1}, {2,2}, {1,4}, {2,4}, {3,4}, {4,4}, {3,3},
	{2,3}, {0,2}, {1,2}, {0,1}, {0,4}, {1,3}, {1,4}, {1,0}
};

static byte ink_pair(byte ink)
{
	return ink == 4 ? 3 : ink;
}

static bool fourth_ink(byte shade)
{
	return mix[shade][0] == 4 || mix[shade][1] == 4;
}

static void make_font(void)
{
	for (byte shade = 0; shade < 16; ++shade)
	{
		byte a = mix[shade][0], b = mix[shade][1];
		// A character can select PF2 or PF3. Use PF1 for the second
		// color when the desired pair requires both of those inks.
		if (fourth_ink(shade))
		{
			if (a == 3) a = 2;
			if (b == 3) b = 2;
		}
		a = ink_pair(a);
		b = ink_pair(b);
		for (byte y = 0; y < 8; ++y)
		{
			byte line = y & 1 ? (b << 2) | a : (a << 2) | b;
			line |= line << 4;
			font[8 * shade + y] = line;
			// The two halves of the checker alternate every two scanlines.
			font[8 * (16 + shade) + y] = line & ((y & 2) ? 0x33 : 0xcc);
		}
	}
}

int main(void)
{
	make_font();
	ColumnDisplayState saved;
	column_display(&saved, DL_MODE4, font, 0, 0);
	os_color[0] = COLOR_WHITE;
	os_color[1] = COLOR_RED;
	os_color[2] = COLOR_YELLOW;
	os_color[3] = COLOR_BLUE;
	os_color[4] = COLOR_BLACK;

	byte x = 0, phase = 0;
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		phase = (phase + 1) & 7;
		antic.hscrol = 3 - (phase >> 1);
		if (!phase)
		{
			byte next = ++x + FETCH - MARGIN - 1;
			for (byte y = 0; y < ROWS; ++y)
			{
				byte sample = rand();
				byte shade = sample & 15;
				column_put(next, y, (sample & 31) | (fourth_ink(shade) ? 0x80 : 0));
			}
			column_position(x);
		}
	}

	column_close(&saved);
	return 0;
}
