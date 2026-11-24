#include <atari/bitmap_screen.h>
#include <atari/gtia.h>
#include <atari/os.h>

#define WIDTH 160
#define PITCH (WIDTH / 4)
#define ROWS 96
#define SCREEN_BYTES (PITCH * ROWS)
#define SPRITE_WIDTH 12
#define SPRITE_HEIGHT 10
#define SPRITE_Y 59
#define FIRST_X 24
#define LAST_X 122

static byte scenery[SCREEN_BYTES];
static byte screen_a[SCREEN_BYTES];
#pragma align(screen_a, 4096)
static byte screen_b[SCREEN_BYTES];
#pragma align(screen_b, 4096)

static byte list_a[104];
#pragma align(list_a, 1024)
static byte list_b[104];
#pragma align(list_b, 1024)
static const ABitmapPageMemory memory[2] = {
	{screen_a, sizeof(screen_a), list_a, sizeof(list_a)},
	{screen_b, sizeof(screen_b), list_b, sizeof(list_b)}
};
static const ABitmapScreenMode mode = {DL_MODED, DMACTL_PF_NORMAL, PITCH, ROWS, 24};

static const ABitmapPoint ridge[] = {
	{12, 84}, {24, 40}, {53, 48}, {77, 23}, {104, 46}, {139, 42}, {152, 84}
};

static const byte pixels[] = {
	0x00, 0x3c, 0x00,
	0x00, 0xff, 0x00,
	0x03, 0xff, 0xc0,
	0x0f, 0xff, 0xf0,
	0x3f, 0xc3, 0xfc,
	0x3f, 0x00, 0xfc,
	0x0f, 0xc3, 0xf0,
	0x03, 0xff, 0xc0,
	0x00, 0xff, 0x00,
	0x00, 0x3c, 0x00
};
static const ABitmapImage sprite = {
	pixels, sizeof(pixels), SPRITE_WIDTH, SPRITE_HEIGHT, SPRITE_WIDTH / 4, 2
};

static bool prepare(ABitmap * background, ABitmapScreen * screen)
{
	if (!abm_init(background, scenery, sizeof(scenery), PITCH, ROWS, DL_MODED) ||
		!abm_screen_init(screen, memory, &mode))
		return false;

	abm_clear(background, 0);
	abm_fill_polygon(background, ridge, sizeof(ridge) / sizeof(ridge[0]), 1);
	abm_polygon(background, ridge, sizeof(ridge) / sizeof(ridge[0]), 2);

	ABitmapRect full = {0, 0, WIDTH, ROWS};
	for (byte page = 0; page < 2; page++)
	{
		abm_copy_rect(&screen->pages[page], 0, 0, background, &full);
		abm_blit(&screen->pages[page], FIRST_X, SPRITE_Y, &sprite, 0);
	}
	return true;
}

int main(void)
{
	ABitmap background;
	ABitmapScreen screen;
	if (!prepare(&background, &screen))
		return 1;

	os_color[0] = ATARI_COLOR(12, 6);
	os_color[1] = ATARI_COLOR(6, 12);
	os_color[2] = ATARI_COLOR(1, 14);
	os_color[4] = ATARI_COLOR(0, 0);
	os_gprior = 0;
	abm_screen_show(&screen);

	int previous[2] = {FIRST_X, FIRST_X};
	int x = FIRST_X, direction = 1;
	for (;;)
	{
		x += direction;
		if (x == FIRST_X || x == LAST_X)
			direction = -direction;

		byte page = screen.front ^ 1;
		const ABitmap * back = abm_screen_back(&screen);
		int old = previous[page];
		ABitmapRect dirty = {old, SPRITE_Y, SPRITE_WIDTH, SPRITE_HEIGHT};
		abm_copy_rect(back, old, SPRITE_Y, &background, &dirty);
		abm_blit(back, x, SPRITE_Y, &sprite, 0);
		previous[page] = x;

		abm_screen_present(&screen);
	}
}
