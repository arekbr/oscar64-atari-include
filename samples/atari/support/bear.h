#ifndef ATARI_SAMPLE_BEAR_H
#define ATARI_SAMPLE_BEAR_H

#include <atari/colors.h>
#include <atari/pmg.h>
#include "display_state.h"

static const byte bear_art[] = {
	#embed "../../resources/friendlybear.bin"
};
static byte bear_dma[2048];
#pragma align(bear_dma, 2048)
static byte bear_shape[3][21];

static void bear_init(void)
{
	for (byte y = 0; y < 21; ++y)
		for (byte x = 0; x < 12; ++x)
		{
			byte bits = bear_art[3 * y + (x >> 2)];
			if (bits & (0xc0 >> (2 * (x & 3))))
			{
				if (x < 8)
					bear_shape[0][y] |= 0x80 >> x;
				else
				{
					bear_shape[1][y] |= 0x80 >> (x - 8);
					bear_shape[2][y] |= 2 >> ((x - 8) >> 1);
				}
			}
		}
	pmg_init(bear_dma, PMG_1LINE);
	gtia.vdelay = 0;
	for (byte i = 0; i < 4; ++i)
	{
		gtia.sizep[i] = 0;
		os_pcolr[i] = ATARI_COLOR(2, 10);
	}
	os_gprior = PRIOR_P0123_PF0123;
}

static void bear_move(byte player, int x, int y, bool missile)
{
	int py = (byte)y - 18;
	byte cut = py < 0 ? -py : 0;
	PMGShape shape;
	shape.count = x >= 0 && x < 354 && cut < 21 ? 21 - cut : 0;
	shape.y = py < 0 ? 0 : py;
	shape.x = 36 + (x >> 1);
	shape.scanlines = bear_shape[0] + (cut < 21 ? cut : 0);
	pmg_move_player(player, &shape);
	shape.x += 8;
	shape.scanlines = bear_shape[missile ? 2 : 1] + (cut < 21 ? cut : 0);
	if (missile)
		pmg_move_missile(player, &shape);
	else
		pmg_move_player(player + 1, &shape);
}

#endif
