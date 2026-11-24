#include <atari/bitmap_screen.h>
#include <atari/os.h>

#ifndef DISPLAY_MODE
#define DISPLAY_MODE 8
#endif

#if DISPLAY_MODE == 8
#define SCREEN_MODE DL_MODEF
#define GTIA_MODE 0
#elif DISPLAY_MODE == 9
#define SCREEN_MODE ABM_GTIA
#define GTIA_MODE 0x40
#elif DISPLAY_MODE == 10
#define SCREEN_MODE ABM_GTIA
#define GTIA_MODE 0x80
#elif DISPLAY_MODE == 15
#define SCREEN_MODE DL_MODEE
#define GTIA_MODE 0
#else
#error DISPLAY_MODE must be 8, 9, 10 or 15
#endif

#if DISPLAY_MODE == 9
#define FOREGROUND 14
#else
#define FOREGROUND 1
#endif

// The first 96 scanlines end exactly at a 4 KB boundary.
static byte page_a[8192];
#pragma align(page_a, 4096)
static byte page_b[8192];
#pragma align(page_b, 4096)

static byte list_a[224];
#pragma align(list_a, 256)
static byte list_b[224];
#pragma align(list_b, 256)

static const ABitmapPageMemory pages[2] = {
	{page_a + 256, 7680, list_a, sizeof(list_a)},
	{page_b + 256, 7680, list_b, sizeof(list_b)}
};

static const ABitmapScreenMode mode = {SCREEN_MODE, DMACTL_PF_NORMAL, 40, 192, 24};


int main(void)
{
	ABitmapScreen screen;
	if (!abm_screen_init(&screen, pages, &mode))
		return 1;
	os_color[0] = 0x48;
	os_color[1] = 0x0e;
	os_color[2] = 0;
	os_color[4] = 0x22;
	os_pcolr[0] = 0x24;
	os_pcolr[1] = 0x6e;
	os_gprior = GTIA_MODE;

	abm_clear(&screen.pages[0], FOREGROUND);
	abm_clear(&screen.pages[1], 0);
	abm_fill_rect(&screen.pages[1], 0, 0, screen.pages[1].width, 32, FOREGROUND);
	abm_fill_rect(&screen.pages[1], 0, 96, screen.pages[1].width, 32, FOREGROUND);
	abm_fill_rect(&screen.pages[1], 0, 188, screen.pages[1].width, 4, FOREGROUND);
	abm_screen_show(&screen);

	for (;;)
	{
		os_wait_frames(16);
		abm_screen_present(&screen);
	}
}
