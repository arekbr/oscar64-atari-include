#include <atari/char_sprite_animation.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})

static const byte frame0[16] =
{
	0,0, 0x05,0x50, 0x15,0x54, 0x55,0x55, 0x55,0x55, 0x15,0x54, 0x05,0x50, 0,0
};
static const byte frame1[16] =
{
	0,0, 0x0a,0xa0, 0x2a,0xa8, 0xaa,0xaa, 0xaa,0xaa, 0x2a,0xa8, 0x0a,0xa0, 0,0
};
static const ACharSpriteFrame frames[2] = {{frame0, sizeof(frame0), 5}, {frame1, sizeof(frame1), 5}};
static const ACharSpriteClip clip = {frames, 2, 2, 8, 2, CHAR_SPRITE_LOOP};
static byte background[960], fixed_font[128];
static volatile byte video[2][1024];
#pragma align(video, 1024)
static volatile byte fonts[2][1024];
#pragma align(fonts, 1024)
static volatile byte lists[2][64];
#pragma align(lists, 1024)
static ACharSpriteScreen screens[2];
static ACharSpriteTrack tracks[2];
static ACharSprite work[2];

int main(void)
{
	for (byte i = 0; i < 2; i++)
	{
		ACharSpriteScreen screen = {background, fixed_font, video[i], fonts[i], 960, 1024, 40, 24, 24, 16};
		screens[i] = screen;
		if (!char_sprite_prepare(&screens[i]) || !char_sprite_play(&tracks[i], &clip))
			return 1;
		volatile byte * list = dl_blank(lists[i], 24);
		list = dl_lms(list, DL_MODE4, video[i]);
		list = dl_mode(list, DL_MODE4, 23);
		dl_jvb(list, lists[i]);
	}
	tracks[0].sprite.x = 8;
	tracks[0].sprite.y = 78;
	tracks[0].velocity_x = 1;
	tracks[1].sprite.x = 140;
	tracks[1].sprite.y = 83;
	tracks[1].velocity_x = -1;
	os_color[0] = 0x46;
	os_color[1] = 0x9a;
	os_color[2] = 0x0e;
	os_color[4] = 0;
	byte target = 0;
	for (;;)
	{
		if (char_sprite_tracks(&screens[target], tracks, 2, work, 2) != CHAR_SPRITE_OK)
			return 2;
		os_wait_frame();
		os_chbas = (word)&fonts[target][0] >> 8;
		os_sdlst = (word)&lists[target][0];
		os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
		os_wait_frame();
		target ^= 1;
		for (byte i = 0; i < 2; i++)
		{
			char_sprite_step(&tracks[i], 1);
			if (tracks[i].sprite.x == 8 || tracks[i].sprite.x == 140)
				tracks[i].velocity_x = -tracks[i].velocity_x;
		}
	}
}
