#include <atari/char_sprite_video.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})

static byte background[960], font[128];
static volatile byte output[1024];
#pragma align(output, 1024)
static volatile byte banks[2048];
#pragma align(banks, 1024)
static volatile byte list[96];
#pragma align(list, 1024)

static const byte shape[16] = {0,0, 0x05,0x50, 0x15,0x54, 0x55,0x55,
	0x55,0x55, 0x15,0x54, 0x05,0x50, 0,0};

int main(void)
{
	ACharSpriteScreen screen = {background, font, output, banks, 960, 2048, 40, 24, 12, 16};
	ACharSpriteVideo video = {&screen, list, sizeof(list), DL_MODE4, 16};
	ACharSprite sprites[2] =
	{
		{shape, sizeof(shape), 48, 92, 2, 8, 2},
		{shape, sizeof(shape), 96, 118, 2, 8, 2}
	};
	os_color[0] = 0x46;
	os_color[1] = 0x9a;
	os_color[2] = 0x0e;
	os_color[4] = 0;
	if (!char_sprite_prepare(&screen) || char_sprite_render(&screen, sprites, 2) != CHAR_SPRITE_OK ||
		!char_sprite_video_show(&video))
		return 1;
	for (;;)
		os_wait_frame();
}
