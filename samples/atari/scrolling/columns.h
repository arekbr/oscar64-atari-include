#ifndef ATARI_SAMPLE_SCROLL_COLUMNS_H
#define ATARI_SAMPLE_SCROLL_COLUMNS_H

#include <atari/dlist.h>
#include <atari/input.h>
#include "../support/display_state.h"

#define ROWS 25
#define RING 64
#define FETCH 48
#define MARGIN 4

static volatile byte columns[ROWS][2 * RING];
#pragma align(columns, 4096)
static byte column_list[3 + 3 * ROWS + 3];
#pragma align(column_list, 1024)
static volatile byte * rows_lms;

struct ColumnDisplayState
{
	BitmapDemoState video;
	byte chbase;
};

static void column_put(byte x, byte y, byte ch)
{
	x &= RING - 1;
	columns[y][x] = ch;
	columns[y][RING + x] = ch;
}

static void column_position(byte left)
{
	left = (left - MARGIN) & (RING - 1);
	volatile byte * list = rows_lms;
	for (byte y = 0; y < ROWS; ++y)
	{
		word address = (word)&columns[y][left];
		list[1] = (byte)address;
		list[2] = address >> 8;
		list += 3;
	}
}

static void column_display(ColumnDisplayState * saved, byte mode, const byte * font, byte dli_first, byte dli_end)
{
	bitmap_demo_save(&saved->video);
	saved->chbase = os_chbas;
	rows_lms = dl_blank(column_list, 24);
	volatile byte * list = rows_lms;
	for (byte y = 0; y < ROWS; ++y)
	{
		byte flags = (y >= dli_first && y < dli_end) ? DL_DLI : 0;
		list = dl_lms(list, mode | DL_HSCROL | flags, columns[y]);
	}
	dl_jvb(list, column_list);
	column_position(0);
	antic.hscrol = 3;
	os_chbas = (word)font >> 8;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(column_list);
}

static void column_close(const ColumnDisplayState * saved)
{
	os_chbas = saved->chbase;
	bitmap_demo_restore(&saved->video);
}

#endif
