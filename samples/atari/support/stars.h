#ifndef ATARI_SAMPLE_STARS_H
#define ATARI_SAMPLE_STARS_H

#include <atari/bitmap_canvas.h>
#include <atari/colors.h>
#include <atari/input.h>
#include <math.h>
#include "bitmap_demo.h"
#include "stipple.h"

static ABitmapCanvas canvas;
static ABitmapPoint star[10];

static void polygon_brush(byte color, const byte * pattern)
{
	abm_canvas_brush(&canvas, color, pattern);
	abm_canvas_fill_polygon(&canvas, star, 10);
}

static void stars(bool multicolor)
{
	ABitmapRect clip = {11, 11, 298, 178};
	abm_canvas_init(&canvas, &screen);
	if (multicolor)
	{
		os_color[0] = COLOR_BLUE;
		os_color[1] = COLOR_YELLOW;
		os_color[2] = COLOR_RED;
		abm_clear(&screen, 1);
		abm_fill_rect(&screen, 4, 8, 152, 184, 2);
		abm_fill_rect(&screen, 5, 10, 150, 180, 0);
		clip = (ABitmapRect){5, 10, 150, 180};
	}
	else
	{
		os_color[4] = COLOR_WHITE;
		for (byte y = 0; y < 200; ++y)
			for (byte x = 0; x < 40; ++x)
				abm_row(&screen, y)[x] = gray[4][y & 7];
		abm_fill_rect(&screen, 14, 14, 304, 184, 0);
		abm_fill_rect(&screen, 8, 8, 304, 184, 1);
		abm_fill_rect(&screen, 10, 10, 300, 180, 0);
	}
	abm_canvas_clip(&canvas, &clip);

	float px[10], py[10];
	for (byte i = 0; i < 10; ++i)
	{
		float angle = i * PI / 5;
		float radius = (i & 1) ? 1.0 : 0.4;
		px[i] = radius * cos(angle);
		py[i] = radius * sin(angle);
	}

	for (byte i = 0; i < 128; ++i)
	{
		float r = i + 4;
		float w = i * PI / 16;
		float c = r * cos(w), s = r * sin(w);
		float cw = r * cos(w * 2.0), sw = r * sin(w * 2.0);
		for (byte j = 0; j < 10; ++j)
		{
			int x = (int)(160 + cw + px[j] * c + py[j] * s);
			star[j].x = multicolor ? x >> 1 : x;
			star[j].y = (int)(100 + sw - px[j] * s + py[j] * c);
		}

		abm_canvas_pen(&canvas, 0, 0, 0);
		byte complement[8];
		if (multicolor)
		{
			for (byte y = 0; y < 8; ++y)
				complement[y] = ~gray[4][y];
			polygon_brush(i & 3, gray[4]);
			polygon_brush((i >> 2) & 3, complement);
		}
		else
		{
			byte shade = i % 9;
			for (byte y = 0; y < 8; ++y)
				complement[y] = ~gray[shade][y];
			polygon_brush(1, gray[shade]);
			polygon_brush(0, complement);
		}
		abm_canvas_pen(&canvas, !multicolor, 0xffff, 1);
		abm_canvas_polygon(&canvas, star, 10);
	}
}

#endif
