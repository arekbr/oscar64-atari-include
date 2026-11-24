#ifndef ATARI_SAMPLE_MANDELBROT_COLOR_CELLS_H
#define ATARI_SAMPLE_MANDELBROT_COLOR_CELLS_H

#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/input.h>
#include <string.h>
#include "../support/bitmap_demo.h"

#define COLUMNS 40
#define ROWS 25

static byte cells[ROWS][COLUMNS];
#pragma align(cells, 1024)
static byte cell_list[3 * ROWS * 8 + 6];
#pragma align(cell_list, 1024)

// Two mode-10 pixels make one cell. Mix palette entries for the extra shades.
static const byte pixels[] = {
	0x88, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x16, 0x17, 0x10, 0x78, 0x77, 0x46, 0x52, 0x70
};

struct ColorCellsState
{
	BitmapDemoState video;
	byte players[4];
};

static void show_cells(ColorCellsState * saved)
{
	bitmap_demo_save(&saved->video);
	for (byte i = 0; i < 4; ++i)
		saved->players[i] = os_pcolr[i];
	memset(cells, pixels[0], sizeof(cells));

	volatile byte * end = dl_blank(cell_list, 24);
	for (byte y = 0; y < ROWS; ++y)
		for (byte line = 0; line < 8; ++line)
			end = dl_lms(end, DL_MODEF, cells[y]);
	dl_jvb(end, cell_list);

	os_pcolr[0] = COLOR_WHITE;
	os_pcolr[1] = COLOR_RED;
	os_pcolr[2] = COLOR_CYAN;
	os_pcolr[3] = COLOR_PURPLE;
	os_color[0] = COLOR_GREEN;
	os_color[1] = COLOR_BLUE;
	os_color[2] = COLOR_YELLOW;
	os_color[3] = COLOR_GRAY2;
	os_color[4] = COLOR_BLACK;
	os_gprior = PRIOR_GTIA_MODE10;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(cell_list);
}

static void close_cells(const ColorCellsState * saved)
{
	for (byte i = 0; i < 4; ++i)
		os_pcolr[i] = saved->players[i];
	bitmap_demo_restore(&saved->video);
}

#endif
