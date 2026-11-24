#include <atari/char_sprite_video.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})

#define STATUS (*(volatile byte *)0x0600)
#define CRITIC (*(volatile byte *)0x0042)
#ifndef SPRITE_MODE
#define SPRITE_MODE DL_MODE4
#endif
#define ROWS (SPRITE_MODE == DL_MODE5 ? 12 : 24)
#define SCALE (SPRITE_MODE == DL_MODE5 ? 2 : 1)

static byte background[960], font[128];
static volatile byte output[1024];
#pragma align(output, 1024)
static volatile byte banks[2048];
#pragma align(banks, 1024)
static volatile byte list[96];
#pragma align(list, 1024)
static volatile byte players[2048];
#pragma align(players, 2048)
static volatile byte next_players[2048];
#pragma align(next_players, 2048)
static ACharSpritePmgFrame frame, next_frame;

static const byte stripes[16] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
	0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};

int main(void)
{
	ACharSpriteScreen screen = {background, font, output, banks, sizeof(background), sizeof(banks), 40, ROWS, ROWS / 2, 16};
	ACharSpriteVideo video = {&screen, list, sizeof(list), SPRITE_MODE, 16};
	ACharSprite sprites[3] =
	{
		{stripes, sizeof(stripes), 48, 0, 2, 8, 2},
		{stripes, sizeof(stripes), 96, 0, 2, 8, 2},
		{stripes, sizeof(stripes), 60, ROWS * 4 - 3, 2, 8, 2}
	};
	ACharSpritePmgActor actors[3] = {{sprites, 0x46, 0x8a}, {sprites + 1, 0xca, 0x26}, {sprites + 2, 0x6c, 0xb8}};
	frame.players = players;
	next_frame.players = next_players;
	STATUS = 0x11;
	os_color[0] = 0x0e;
	os_color[1] = 0x06;
	os_color[4] = 0;
	for (byte row = 0; row < ROWS; row++)
		background[(word)row * 40 + 1] = 1;
	if (!char_sprite_prepare(&screen) || char_sprite_render(&screen, 0, 0) != CHAR_SPRITE_OK ||
		char_sprite_pmg_render(&frame, &video, actors, 3) != CHAR_SPRITE_OK)
		return 1;
	for (byte row = 0; row < 8; row++)
	{
		banks[8 + row] = 0x55;
		banks[1032 + row] = 0xaa;
	}
	if (players[0x418] != 0xf0 || players[0x518] != 0x0f || players[0x618] != 0xf0 || players[0x718] != 0x0f ||
		frame.color[0][0] != 0x46 || frame.color[2][0] != 0xca)
		return 2;
	STATUS = 0x22;
	if (!char_sprite_video_show_pmg(&video, &frame) || char_sprite_video_update_pmg(&frame) ||
		char_sprite_pmg_render(&frame, &video, actors, 3) != CHAR_SPRITE_BAD)
		return 3;
	os_wait_frames(12);
	sprites[2].x = 64;
	actors[2].first = 0x36;
	STATUS = 0x33;
	if (char_sprite_pmg_render(&next_frame, &video, actors, 3) != CHAR_SPRITE_OK)
		return 4;
	CRITIC = 1;
	bool published = char_sprite_video_update_pmg(&next_frame);
	CRITIC = 0;
	if (!published)
		return 4;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
