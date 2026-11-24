#include <atari/colors.h>
#include <stdlib.h>
#include "columns.h"

static byte font[1024];
#pragma align(font, 1024)

// Top and bottom row of the tunnel

static byte ytop[256], ybottom[256];
static byte rbuff[ROWS];

// Prepare one column of the tunnel

static void prepcol(byte xi)
{
	char		yt, yb;
	signed char dyt, dyb;

	// Current height of top and bottom
	yt = ytop[(char)(xi + 0)];
	yb = ybottom[(char)(xi + 0)];

	// Height of column to the left for diagonal
	dyt = yt - ytop[(char)(xi - 1)];
	dyb = yb - ybottom[(char)(xi - 1)];

	// Fill top, center and bottom range
	for(char i=0; i<yt; i++)
		rbuff[i] = 1;
	for(char i=yt; i<yb; i++)
		rbuff[i] = 0;
	for(char i=yb; i<25; i++)
		rbuff[i] = 1;

	// Select transitional characters based on slope
	if (dyt < 0 && yt < ROWS)
		rbuff[yt] = 2;
	else if (dyt > 0 && yt > 0)
		rbuff[yt - 1] = 3;

	if (dyb < 0 && yb < ROWS)
		rbuff[yb] = 4;
	else if (dyb > 0 && yb > 0)
		rbuff[yb - 1] = 5;

}

// Initialize tunnel with "random" data
static void build_tunnel(void)
{
	signed char yt = 1, yb = 24, dyt = 1, dyb = -1;

	for(int i=0; i<256; i++)
	{
		unsigned r = rand();

		if (!(r & 0x00e0))
			dyt = -dyt;
		if (!(r & 0xe000))
			dyb = -dyb;

		yt += dyt;
		yb += dyb;
		if (yt < 0)
		{
			yt = 0;
			dyt = 1;
		}
		if (yb > 25)
		{
			yb = 25;
			dyb = -1;
		}

		ytop[i] = yt;
		ybottom[i] = yb;

		if (yt + 5 > yb)
		{
			dyt = -1;
			dyb = 1;
		}
	}

}

// The two diagonal characters and their inverse forms join adjacent steps.
static void make_font(void)
{
	for (byte y = 0; y < 8; ++y)
	{
		font[8 + y] = 0xff;
		font[16 + y] = 0xff << y;
		font[24 + y] = 0xff >> y;
		font[32 + y] = ~font[16 + y];
		font[40 + y] = ~font[24 + y];
	}
}

static void show_column(byte x)
{
	prepcol(x);
	for (byte y = 0; y < ROWS; ++y)
		column_put(x, y, rbuff[y]);
}

int main(void)
{
	make_font();
	build_tunnel();
	for (byte x = 0; x < FETCH; ++x)
		show_column(x - MARGIN);

	ColumnDisplayState saved;
	column_display(&saved, DL_MODE2, font, 0, 0);
	os_color[1] = COLOR_WHITE;
	os_color[2] = ATARI_COLOR(HUE_YELLOW, 0);
	os_color[4] = COLOR_BLACK;

	word position = 0;
	int dx = 0, ax = 1;
	byte previous = 0;
	while (!(cons_keys() & CONSOL_START))
	{
		if ((rand() & 127) == 0)
			ax = -ax;
		dx += ax;
		if (dx > 32)
			dx = 32;
		else if (dx < -32)
			dx = -32;

		position += dx;
		byte left = position >> 5;
		os_wait_frame();
		if (left != previous)
		{
			show_column(dx > 0 ? left + FETCH - MARGIN - 1 : left - MARGIN);
			column_position(left);
			previous = left;
		}
		antic.hscrol = 3 - ((position >> 3) & 3);
	}

	column_close(&saved);
	return 0;
}
