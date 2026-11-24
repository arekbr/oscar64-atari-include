#ifndef ATARI_SAMPLE_SHMUP_VIDEO_H
#define ATARI_SAMPLE_SHMUP_VIDEO_H

#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/irq.h>
#include <atari/pmg.h>
#include <string.h>
#include "../support/display_state.h"
#include "../support/c64_palette.h"

#define ROWS 20
#define PITCH 52
#define BANKS (ROWS / 2)
#define SHOT_GLYPHS 18
#define STAR (2 * SHOT_GLYPHS)
#define FIRST_TILE (STAR + 1)
#define FIRST_LINE 56

static const byte world_chars[] = {
	#embed "../../resources/hscrollshmupchars.bin"
};
static const byte text_chars[] = {
	#embed "../../resources/breakoutchars.bin"
};
static const byte tiles[] = {
	#embed "../../resources/hscrollshmuptiles.bin"
};
static const byte map[] = {
	#embed "../../resources/hscrollshmupmap.bin"
};
static const byte sprite_data[] = {
	#embed "../../resources/hscrollshmupsprites.bin"
};

static byte hud[40];
#pragma align(hud, 64)
static byte fonts[BANKS][1024];
#pragma align(fonts, 1024)
static byte bank_page[BANKS], blank_ch[BANKS];
static byte status_font[1024];
#pragma align(status_font, 1024)
static byte tile_bank[BANKS][64][8];
static byte playfield[2][ROWS][PITCH];
#pragma align(playfield, 4096)
static byte lists[2][128];
#pragma align(lists, 1024)
static volatile byte * lms[2];
static byte dma[2048];
#pragma align(dma, 2048)
static byte screen_page, fine;

struct ShmupSprite
{
	int x, y;
	byte image, color;
	bool active;
};

static ShmupSprite sprites[8];
static byte enemy_x[5], enemy_color[5];
static byte prev_enemy[5];
static byte player_mask[2][21];
static byte shadow_y, shadow_rows;

static void spr_set(byte i, bool show, int x, int y, byte image, byte color,
		bool multicolor, bool double_x, bool double_y)
{
	sprites[i] = (ShmupSprite){x, y, image, color, show};
}
static void spr_show(byte i, bool active) { sprites[i].active = active; }
static void spr_move(byte i, int x, int y) { sprites[i].x = x; sprites[i].y = y; }
static void spr_color(byte i, byte color) { sprites[i].color = color; }
static void spr_image(byte i, byte image) { sprites[i].image = image; }

static void sprite_mask(byte image, volatile byte * left, volatile byte * right)
{
	const byte * src = sprite_data + 64 * (image & 63);
	for (byte y = 0; y < 21; ++y)
	{
		byte a = 0, b = 0;
		for (byte x = 0; x < 4; ++x)
		{
			byte mask = 0xc0 >> (2 * x);
			if (src[0] & mask) a |= 0x80 >> x;
			if (src[1] & mask) a |= 0x08 >> x;
			if (src[2] & mask) b |= 0x80 >> x;
		}
		left[y] = a;
		right[y] = b;
		src += 3;
	}
}

// Three double-width missiles form the shadow under the two ship players.
static void draw_shadow(void)
{
	volatile byte * plane = pmg_missiles();
	for (byte y = 0; y < shadow_rows; ++y)
		plane[shadow_y + y] = 0;

	ShmupSprite * shadow = &sprites[7];
	shadow_rows = 0;
	if (!shadow->active)
		return;

	byte x = 36 + (shadow->x >> 1);
	for (byte m = 0; m < 3; ++m)
		gtia.hposm[m] = x + 4 * m;
	os_color[3] = palette[shadow->color];

	shadow_y = shadow->y - 18;
	shadow_rows = 21;
	const byte * src = sprite_data + 64 * (shadow->image & 63);
	for (byte y = 0; y < shadow_rows; ++y)
	{
		byte bits = 0;
		for (byte m = 0; m < 3; ++m)
		{
			byte mask = (src[m] & 0xf0 ? 2 : 0) | (src[m] & 0x0f ? 1 : 0);
			bits |= mask << (2 * m);
		}
		plane[shadow_y + y] = bits;
		src += 3;
	}
}

static void draw_sprites(void)
{
	ShmupSprite * ship = &sprites[0];
	PMGShape shape = {player_mask[0], 0, 0, 0};
	if (ship->active)
	{
		sprite_mask(ship->image, player_mask[0], player_mask[1]);
		os_pcolr[0] = os_pcolr[1] = palette[ship->color];
		shape.x = 36 + (ship->x >> 1);
		shape.y = ship->y - 18;
		shape.count = 21;
	}
	pmg_move_player(0, &shape);
	shape.x += 8;
	shape.scanlines = player_mask[1];
	pmg_move_player(1, &shape);

	for (byte i = 0; i < 5; ++i)
	{
		ShmupSprite * enemy = &sprites[i + 2];
		byte image = enemy->active ? enemy->image : 0xff;
		// An enemy stays in the same 32-scanline band for its lifetime.
		if (image != prev_enemy[i])
		{
			byte y = 61 + 32 * i;
			volatile byte * a = pmg_player(2) + y;
			volatile byte * b = pmg_player(3) + y;
			if (image == 0xff)
				for (byte j = 0; j < 21; ++j) a[j] = b[j] = 0;
			else
				sprite_mask(image, a, b);
			prev_enemy[i] = image;
		}
		enemy_x[i] = enemy->active && enemy->x >= -10 && enemy->x < 346 ?
			36 + (enemy->x >> 1) : 0;
		enemy_color[i] = palette[enemy->color];
	}
}

// The two player pairs use fixed vertical planes; DLI changes enemy X and ink.
__asm playfield_dli
{
		pha
		txa
		pha
		cld
		lda $d40b
		sec
		sbc #((FIRST_LINE - 1) / 2)
		lsr
		lsr
		lsr
		tax
		sta $d40a
		lda bank_page,x
		sta $d409
		txa
		lsr
		tax
		lda enemy_x,x
		sta $d002
		beq no_enemy
		clc
		adc #8
no_enemy:
		sta $d003
		lda enemy_color,x
		sta $d014
		sta $d015
		lda #0x0e
		sta $d01a
		lda #0x06
		sta $d017
		lda #0
		sta $d018
		pla
		tax
		pla
		rti
}

static void make_banks(void)
{
	memcpy(status_font, text_chars, 1024);
	for (byte bank = 0; bank < BANKS; ++bank)
	{
		static byte ids[166];
		memset(ids, 0xff, sizeof(ids));
		bank_page[bank] = (word)fonts[bank] >> 8;
		byte next = FIRST_TILE;
		const byte * tiles_in_band = map + (word)(bank >> 1) * 128;
		for (byte i = 0; i < 128; ++i)
		{
			byte tile = tiles_in_band[i];
			const byte * art = tiles + 16 * tile + ((bank & 1) << 3);
			for (byte pixel = 0; pixel < 8; ++pixel)
			{
				byte ch = art[pixel];
				if (ids[ch] == 0xff)
				{
					ids[ch] = next;
					memcpy(fonts[bank] + 8 * next, world_chars + 8 * ch, 8);
					++next;
				}
				tile_bank[bank][tile][pixel] = ids[ch];
			}
		}
		blank_ch[bank] = ids[0];
		fonts[bank][STAR * 8 + 2] = 0x30;
	}
}

static void make_display(void)
{
	for (byte page = 0; page < 2; ++page)
	{
		volatile byte * p = dl_blank(lists[page], 24);
		p = dl_lms(p, DL_MODE2, hud);
		*p++ = DL_BLANK8;
		*p++ = DL_BLANK8 | DL_DLI;
		lms[page] = p;
		for (byte row = 0; row < ROWS; ++row)
		{
			byte mode = DL_MODE4 | DL_HSCROL;
			if ((row & 1) && row != ROWS - 1)
				mode |= DL_DLI;
			p = dl_lms(p, mode, playfield[page][row]);
		}
		dl_jvb(p, lists[page]);
	}
}

static void draw_world(word x)
{
	fine = 3 - ((x >> 1) & 3);
	byte origin = (x >> 3) - 4;
	// The eighth bit is the high bit of the extended scrolling map column.
	word column = ((x >> 3) - 4) & 511;
	byte tile_start = column >> 2;
	byte phase = origin & 3;
	for (byte y = 0; y < ROWS; ++y)
	{
		byte * dst = playfield[screen_page][y];
		const byte * map_row = map + 128 * (word)(y >> 2);
		byte tile = tile_start;
		for (byte i = 0; i < 13; ++i)
		{
			const byte * colors = tile_bank[y >> 1][map_row[tile]] + ((y & 1) << 2);
			dst[0] = colors[0];
			dst[1] = colors[1];
			dst[2] = colors[2];
			dst[3] = colors[3];
			dst += 4;
			tile = (tile + 1) & 127;
		}
		word address = (word)(playfield[screen_page][y] + phase);
		lms[screen_page][3 * y + 1] = (byte)address;
		lms[screen_page][3 * y + 2] = address >> 8;
	}
}

static void show_video(void)
{
	make_banks();
	make_display();
	memset(prev_enemy, 0xff, sizeof(prev_enemy));
	os_chbas = (word)status_font >> 8;
	os_color[0] = COLOR_GRAY3;
	os_color[1] = COLOR_WHITE;
	os_color[2] = ATARI_COLOR(HUE_YELLOW, 0);
	os_color[4] = COLOR_BLACK;
	os_gprior = PRIOR_P0123_PF0123 | PRIOR_FIFTH_PLAYER;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	pmg_init(dma, PMG_1LINE);
	for (byte i = 0; i < 4; ++i) gtia.sizep[i] = 0;
	gtia.vdelay = 0;
	gtia.sizem = 0x15;
}

static void present_video(void)
{
	// Both display lists share their high byte. Publish by changing the
	// low byte of SDLST during vertical blank.
	os_wait_frame();
	os_sdlst = (word)lists[screen_page];
	antic.dlist = os_sdlst;
	antic.hscrol = fine;
	draw_sprites();
	draw_shadow();
	screen_page ^= 1;
}

#endif
