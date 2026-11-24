#include <atari/colors.h>
#include <atari/input.h>
#include <stdio.h>
#include <stdlib.h>
#include "../support/bitmap_demo.h"

#define COUNT 160
static byte field[COUNT];

static void draw(byte index)
{
	word x = 2 * index;
	byte y = field[index];
	abm_line(&screen, x, 0, x, y, 0);
	abm_line(&screen, x, y, x, COUNT, 1);
}

static void partition(int left, int right)
{
	while (left < right)
	{
		int i = left, j = right;
		byte pivot = field[(left + right) / 2];
		while (i <= j)
		{
			while (field[i] > pivot)
				++i;
			while (field[j] < pivot)
				--j;
			if (i <= j)
			{
				byte temp = field[i];
				field[i] = field[j];
				field[j] = temp;
				draw(i++);
				draw(j--);
			}
		}
		partition(left, j);
		left = i;
	}
}

static word frame_ticks(void)
{
	byte high, low;
	do
	{
		high = os_rtclok[1];
		low = os_rtclok[2];
	} while (high != os_rtclok[1]);
	return (word)high << 8 | low;
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
	for (byte i = 0; i < COUNT; ++i)
		field[i] = i;
	for (byte i = 0; i < COUNT; ++i)
	{
		byte j = rand() % COUNT;
		byte temp = field[i];
		field[i] = field[j];
		field[j] = temp;
	}
	for (byte i = 0; i < COUNT; ++i)
		draw(i);
	word begin = frame_ticks();
	partition(0, COUNT - 1);
	word elapsed = frame_ticks() - begin;
	char label[24];
	sprintf(label, "TIME : %.1f SECS.", (float)elapsed / (os_palnts ? 50 : 60));
	abm_text(&screen, 4, 170, label, (const byte *)0xe000, 1);
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
