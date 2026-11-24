#ifndef ATARI_SAMPLE_BITMAP_SPRITES_H
#define ATARI_SAMPLE_BITMAP_SPRITES_H

#include <atari/bitmap_screen.h>
#include <atari/colors.h>
#include <atari/input.h>
#include <string.h>
#include "../support/display_state.h"
#include "../support/c64_palette.h"

#pragma region(main, 0x2080, 0x6010, , , {code, data, bss, heap, stack})
#pragma stacksize(768)
#pragma heapsize(0)

#define WIDTH 80
#define ROWS 200
#define SHAPE_ROWS 21
#define SHAPE_WIDTH 6
#ifdef BALL_SPRITES
#define SHAPES 8
static const byte original[] = {
	#embed "../../resources/ballsprite.bin"
};
#else
#define SHAPES 16
static const byte original[] = {
	#embed "../../resources/digitsprites.bin"
};
#endif

struct Stamp
{
	byte image[4], mask[4];
};
static Stamp masks[SHAPES][2][SHAPE_ROWS];
static byte lists[2][1024];
#pragma align(lists, 1024)
static const ABitmapPageMemory buffers[2] = {
	{(volatile byte *)0x6010, 8000, lists[0], sizeof(lists[0])},
	{(volatile byte *)0x8010, 8000, lists[1], sizeof(lists[1])}
};
static ABitmapScreen display;
static const ABitmap * canvas;

struct SpriteVideoState
{
	BitmapDemoState video;
	byte colors[4];
};

static void make_stamps(void)
{
	for (byte index = 0; index < SHAPES; ++index)
		for (byte y = 0; y < SHAPE_ROWS; ++y)
		{
#ifdef BALL_SPRITES
			const byte * p = original + 3 * y;
#else
			const byte * p = original + 64 * index + 3 * y;
#endif
			for (byte x = 0; x < SHAPE_WIDTH; ++x)
			{
				byte nibble = (p[x >> 1] >> ((x & 1) ? 0 : 4)) & 15;
				byte ink = 0;
#ifdef BALL_SPRITES
				byte a = nibble >> 2, b = nibble & 3;
				if (a == 2 || b == 2) ink = 8;
				if (a == 3 || b == 3) ink = 1 + index;
#else
				if (nibble) ink = 1 + (index & 7);
#endif
				if (ink)
					for (byte shift = 0; shift < 2; ++shift)
					{
						Stamp * stamp = &masks[index][shift][y];
						byte pixel = x + shift;
						byte bits = (pixel & 1) ? 0 : 4;
						stamp->image[pixel >> 1] |= ink << bits;
						stamp->mask[pixel >> 1] |= 15 << bits;
					}
			}
		}
}

static bool start_sprites(SpriteVideoState * saved)
{
	bitmap_demo_save(&saved->video);
	for (byte i = 0; i < 4; ++i) saved->colors[i] = os_pcolr[i];
	const ABitmapScreenMode mode = {ABM_GTIA, DMACTL_PF_NORMAL, 40, ROWS, 24};
	if (!abm_screen_init(&display, buffers, &mode))
		return false;
	make_stamps();
	for (byte i = 0; i < 2; ++i)
		memset((void *)display.pages[i].data, 0, 8000);

	os_pcolr[0] = COLOR_BLACK;
	for (byte i = 1; i < 4; ++i)
		os_pcolr[i] = palette[7 + i];
	for (byte i = 0; i < 5; ++i)
		os_color[i] = palette[11 + i];
#ifdef BALL_SPRITES
	os_color[4] = COLOR_WHITE;
#endif
	os_gprior = PRIOR_GTIA_MODE10;
	abm_screen_show(&display);
	return true;
}

static void stop_sprites(const SpriteVideoState * saved)
{
	for (byte i = 0; i < 4; ++i) os_pcolr[i] = saved->colors[i];
	bitmap_demo_restore(&saved->video);
}

static void start_frame(void)
{
	canvas = abm_screen_back(&display);
	memset((void *)canvas->data, 0, 8000);
}

// Convert the VIC's sprite origin and draw one packed GTIA mask.
static void draw_sprite(int x, int y, byte image)
{
	x = (x - 24) >> 2;
	y -= 50;
	if (x <= -SHAPE_WIDTH || x >= WIDTH || y <= -SHAPE_ROWS || y >= ROWS)
		return;
	sbyte bx = x >> 1;
	const Stamp * s = masks[image][x & 1];
	byte top = y >= 0 ? 0 : -y;
	byte bottom = y + SHAPE_ROWS <= ROWS ? SHAPE_ROWS : ROWS - y;
	volatile byte * out = canvas->data + (word)(y + top) * 40;

	for (byte row = top; row < bottom; ++row)
	{
		const Stamp * stamp = &s[row];
		if (bx >= 0 && bx <= 36)
		{
			volatile byte * p = out + bx;
			p[0] = (p[0] & ~stamp->mask[0]) | stamp->image[0];
			p[1] = (p[1] & ~stamp->mask[1]) | stamp->image[1];
			p[2] = (p[2] & ~stamp->mask[2]) | stamp->image[2];
			p[3] = (p[3] & ~stamp->mask[3]) | stamp->image[3];
		}
		else
			for (byte c = 0; c < 4; ++c)
			{
				byte at = bx + c;
				if (at < 40)
					out[at] = (out[at] & ~stamp->mask[c]) | stamp->image[c];
			}
		out += 40;
	}
}

#endif
