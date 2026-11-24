#ifndef ATARI_SAMPLE_SCROLL_GRID_H
#define ATARI_SAMPLE_SCROLL_GRID_H

#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/input.h>
#include <stdlib.h>
#include "../support/display_state.h"

#define MAP_CELLS (4 * MAP_TILES)
#define PITCH 144
#define PAGE_ROWS 28
#define PAGES ((MAP_CELLS + PAGE_ROWS - 1) / PAGE_ROWS)
#define ROWS (25 - TOP_ROWS)

static byte map[PAGES][4096];
#pragma align(map, 4096)
static byte * rows[MAP_CELLS];
static byte font[1024];
#pragma align(font, 1024)
static byte list[81];
#pragma align(list, 1024)
static byte tiles[MAP_TILES][MAP_TILES];
static volatile byte * grid_lms;

// Each tile is four by four characters. Masks describe the strokes and
// filled quadrants used by the scroll map.
static const byte shapes[][8] = {
	{0,0,0,0,0,0,0,0},
	{0,0,0,255,255,0,0,0},
	{0,0,0,224,240,56,24,24},
	{24,24,28,15,7,0,0,0},
	{24,24,56,240,224,0,0,0},
	{192,224,112,56,28,14,7,3},
	{3,7,14,28,56,112,224,192},
	{0,0,0,7,15,28,24,24},
	{24,24,24,24,24,24,24,24},
	{240,240,240,240,240,240,240,240},
	{0,0,0,0,255,255,255,255},
	{192,192,192,192,192,192,192,192},
	{3,3,3,3,3,3,3,3},
	{0,0,0,0,15,15,15,15},
	{0,0,0,0,0,0,255,255},
	{255,255,0,0,0,0,0,0},
	{0,0,0,0,240,240,240,240},
	{15,15,15,15,0,0,0,0},
	{240,240,240,240,0,0,0,0},
	{255,255,255,255,255,255,255,255},
	{15,15,15,15,15,15,15,15},
	{255,255,255,255,0,0,0,0}
};

static const byte quad[4][4 * 4] =
{
	{
		0, 7, 13, 6,
		0, 8, 20, 11,
		0, 8, 20, 11,
		0, 3, 17, 5,
	},
	{
		0, 1, 10, 15,
		0, 0, 19, 0,
		0, 0, 19, 0,
		0, 1, 21, 14,
	},
	{
		0, 1, 10, 15,
		0, 0, 19, 0,
		0, 0, 19, 0,
		0, 1, 21, 14,
	},
	{
		0, 2, 16, 5,
		0, 8, 9, 12,
		0, 8,	9, 12,
		0, 4, 18, 6,
	}
};

static void make_map(void)
{
	for (byte y = 0; y < MAP_TILES; ++y)
		for (byte x = 0; x < MAP_TILES; ++x)
			tiles[y][x] = rand() & 3;

	for (byte y = 0; y < MAP_CELLS; ++y)
	{
		byte * row = map[y / PAGE_ROWS] + PITCH * (y % PAGE_ROWS);
		rows[y] = row;
		for (byte x = 0; x < MAP_CELLS; ++x)
		{
			byte tile = tiles[y >> 2][x >> 2];
			byte ch = quad[x & 3][4 * (y & 3) + tile];
			row[4 + x] = MODE == DL_MODE4 ? 32 * tile + ch : ch;
		}
	}

	for (byte ch = 0; ch < sizeof(shapes) / 8; ++ch)
		for (byte y = 0; y < 8; ++y)
		{
			byte mask = shapes[ch][y];
			if (MODE == DL_MODE2)
				font[8 * ch + y] = mask;
			else
			{
				byte pixels = 0;
				for (byte x = 0; x < 8; x += 2)
					if (mask & (0xc0 >> x))
						pixels |= 0x40 >> x;
				font[8 * (32 + ch) + y] = pixels;
				font[8 * (64 + ch) + y] = pixels << 1;
				font[8 * (96 + ch) + y] = pixels | (pixels << 1);
			}
		}
}

static void grid_position(word x, word y)
{
	volatile byte * row = grid_lms;
	byte cx = x >> 3, cy = y >> 3;
	for (byte i = 0; i < ROWS; ++i)
	{
		word address = (word)(rows[cy + i] + cx);
		row[1] = (byte)address;
		row[2] = address >> 8;
		row += 3;
	}
	antic.hscrol = 3 - ((x >> 1) & 3);
	antic.vscrol = y & 7;
}

static void grid_display(void)
{
	grid_lms = dl_blank(list, 24 + TOP_ROWS * 8);
	volatile byte * p = grid_lms;
	for (byte row = 0; row < ROWS; ++row)
	{
		byte flags = DL_HSCROL | (row == ROWS - 1 ? 0 : DL_VSCROL);
		p = dl_lms(p, MODE | flags, rows[row]);
	}
	dl_jvb(p, list);

	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_WHITE;
	os_color[1] = MODE == DL_MODE4 ? COLOR_RED : COLOR_LIGHTBLUE;
	os_color[2] = MODE == DL_MODE4 ? COLOR_CYAN : COLOR_BLUE;
	os_color[4] = COLOR_BLACK;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
}

#endif
