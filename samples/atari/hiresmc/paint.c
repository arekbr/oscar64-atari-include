#include <atari/colors.h>
#include <atari/console.h>
#include <atari/input.h>
#include <atari/pmg.h>
#ifdef PAINT_MOUSE
#include <atari/pointer.h>
#include <atari/pokey.h>
#endif
#include "../support/bitmap_demo.h"

#define WIDTH 160
#define ROWS 200
static byte work[(WIDTH / 8) * ROWS];

static const byte mouse_art[] = {
	#embed spd_sprites "../../resources/mouse.spd"
};
static byte mask[2][21];
static const byte inks[10][2] = {
	{0,0}, {1,1}, {2,2}, {3,3}, {1,0}, {1,2}, {1,3}, {2,0}, {2,3}, {3,0}
};

static void make_pointer(void)
{
	for (byte plane = 0; plane < 2; ++plane)
		for (byte y = 0; y < 21; ++y)
			for (byte x = 0; x < 8; ++x)
				if (mouse_art[64 * (1 - plane) + 3 * y + (x >> 2)] & (0xc0 >> (2 * (x & 3))))
					mask[plane][y] |= 0x80 >> x;
}

static void show_pointer(int x, int y)
{
	PMGShape pointer = {mask[0], 21, 48 + (x >> 1), 32 + y};
	pmg_move_player(0, &pointer);
	pointer.scanlines = mask[1];
	pmg_move_player(1, &pointer);
}

static void dot(int x, int y, byte ink)
{
	for (sbyte dy = -5; dy <= 5; ++dy)
		for (sbyte dx = -5; dx <= 5; ++dx)
			if (dx * dx + dy * dy <= 25)
			{
				int px = (x + dx) >> 1, py = y + dy;
				if (px >= 0 && px < WIDTH && py >= 0 && py < ROWS)
					abm_put(&screen, px, py, inks[ink][(px + py) & 1]);
			}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte old0 = os_pcolr[0], old1 = os_pcolr[1];
	if (!show_bitmap(DL_MODEE, ROWS))
		return 1;
	os_color[0] = COLOR_BLUE;
	os_color[1] = COLOR_YELLOW;
	os_color[2] = COLOR_RED;
	pmg_init((volatile byte *)0xa000, PMG_1LINE);
	os_pcolr[0] = COLOR_BLACK;
	os_pcolr[1] = COLOR_WHITE;
	gtia.sizep[0] = gtia.sizep[1] = gtia.vdelay = 0;
	os_gprior = PRIOR_P0123_PF0123;
#ifdef PAINT_MOUSE
	pokey.audctl = 0;
	if (!pointer_irq_start(POINTER_ST, 0, 0, 0))
	{
		pmg_off();
		bitmap_demo_restore(&saved);
		return 1;
	}
#endif
	make_pointer();
	int x = 0, y = 0;
	byte ink = 1;
	show_pointer(x, y);
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		byte key = key_pressed() ? key_atascii(key_get()) : KEY_NONE;
		if (key >= '0' && key <= '9')
			ink = key - '0';
		else if (key == 'C' || key == 'c' || key == CH_CLR)
			abm_clear(&screen, 0);
		else if (key == 'F' || key == 'f')
			abm_flood_fill(&screen, x >> 1, y, inks[ink][0], work, sizeof(work));

		sbyte dx = 0, dy = 0;
		bool fire = false;
#ifdef PAINT_MOUSE
		APointerMotion motion;
		if (pointer_irq_take(&motion))
		{
			dx = motion.dx;
			dy = motion.dy;
			fire = (motion.buttons & POINTER_LEFT) != 0;
		}
#else
		byte stick = joy_read(0);
		dx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
		dy = ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);
		fire = joy_fire(0);
#endif
		int nx = x + dx, ny = y + dy;
		nx = nx < 0 ? 0 : nx >= 2 * WIDTH ? 2 * WIDTH - 1 : nx;
		ny = ny < 0 ? 0 : ny >= ROWS ? ROWS - 1 : ny;
		if (nx != x || ny != y)
		{
			x = nx;
			y = ny;
			show_pointer(x, y);
			if (fire)
				dot(x, y, ink);
		}
	}
#ifdef PAINT_MOUSE
	pointer_irq_stop();
#endif
	pmg_off();
	os_pcolr[0] = old0;
	os_pcolr[1] = old1;
	bitmap_demo_restore(&saved);
	return 0;
}
