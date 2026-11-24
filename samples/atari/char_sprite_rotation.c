#include <atari/char_sprite_video.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#ifndef SHOW_ACTOR
#define SHOW_ACTOR 3
#endif

#define STATUS (*(volatile byte *)0x0600)
#define ROWS 24

static byte background[960], font[64];
static volatile byte output[1024];
#pragma align(output, 1024)
static volatile byte banks[1024];
#pragma align(banks, 1024)
static volatile byte list[96];
#pragma align(list, 1024)

static const ACharSpriteScreen screen = {background, font, output, banks, 960, 1024, 40, ROWS, ROWS, 8};
static ACharSpriteVideo video = {&screen, list, sizeof(list), DL_MODE4, 16};
static const byte stripe[] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
	0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};
static const ACharSprite sprites[] =
{
	{stripe, sizeof(stripe), 24, 40, 2, 8, 2},
	{stripe, sizeof(stripe), 54, 40, 2, 8, 2},
	{stripe, sizeof(stripe), 84, 40, 2, 8, 2},
	{stripe, sizeof(stripe), 114, 40, 2, 8, 2}
};
static const ACharSpritePmgActor actors[] =
{
	{sprites, 0x0e, 0x08},
	{sprites + 1, 0x46, 0x48},
	{sprites + 2, 0x86, 0x88},
	{sprites + 3, 0xc6, 0xc8}
};
static ACharSpritePmgFrame frame;

static bool winners(byte other)
{
	return frame.color[0][5] == actors[0].first && frame.color[2][5] == actors[other].first &&
		frame.position[0][5] == 48 + sprites[0].x && frame.position[1][5] == 48 + sprites[other].x &&
		frame.players[0x440] == 0xf0 && frame.players[0x540] == 0x0f &&
		frame.players[0x640] == 0xf0 && frame.players[0x740] == 0x0f;
}

static bool check_rotation(void)
{
	byte next = 0;
	if (char_sprite_pmg_render(&frame, &video, actors, 4) != CHAR_SPRITE_FULL || !winners(1))
		return false;
	for (byte expected = 3; expected; expected--)
		if (char_sprite_pmg_render_rotating(&frame, &video, actors, 4, &next) != CHAR_SPRITE_FULL ||
			!winners(expected) || next != (expected == 1 ? 3 : expected - 1))
			return false;
	if (char_sprite_pmg_render_rotating(&frame, &video, actors, 2, &next) != CHAR_SPRITE_OK ||
		next != 1 || !winners(1))
		return false;
	if (char_sprite_pmg_render_rotating(&frame, &video, 0, 4, &next) != CHAR_SPRITE_BAD || next != 1)
		return false;
	next = SHOW_ACTOR;
	return char_sprite_pmg_render_rotating(&frame, &video, actors, 4, &next) == CHAR_SPRITE_FULL && winners(SHOW_ACTOR);
}

int main(void)
{
	frame.players = (volatile byte *)0x8000;
	STATUS = 1;
	if (!char_sprite_prepare(&screen) || char_sprite_render(&screen, 0, 0) != CHAR_SPRITE_OK)
		return 1;
	STATUS = 2;
	if (!check_rotation())
		return 2;
	STATUS = 3;
	os_color[4] = 0;
	byte next = 2;
	if (!char_sprite_video_show_pmg(&video, &frame) ||
		char_sprite_pmg_render_rotating(&frame, &video, actors, 4, &next) != CHAR_SPRITE_BAD || next != 2)
		return 3;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
