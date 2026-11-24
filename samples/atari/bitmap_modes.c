#include <atari/bitmap.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>

#ifndef BITMAP_MODE
#define BITMAP_MODE DL_MODED
#endif

#ifndef BITMAP_GTIA_MODE
#define BITMAP_GTIA_MODE 9
#endif

#ifndef BITMAP_PLAYFIELD
#define BITMAP_PLAYFIELD DMACTL_PF_NORMAL
#endif

#if BITMAP_PLAYFIELD == DMACTL_PF_WIDE
static byte screen[12288];
#else
static byte screen[8192];
#endif
#pragma align(screen, 4096)

#if BITMAP_PLAYFIELD == DMACTL_PF_WIDE
static byte display_list[582];
#else
static byte display_list[208];
#endif
#pragma align(display_list, 1024)

int main(void)
{
	ABitmapMode info;
	if (!abm_mode_info_width(BITMAP_MODE, BITMAP_PLAYFIELD, &info))
		return 1;

	ABitmap bitmap;
	byte rows = 192 / info.scanlines;
	byte pitch = info.row_bytes;
	// Use a divisor of 4096 when the image needs more than two pages.
	if ((word)rows * pitch > 8192 - 4096 % pitch)
		pitch = 64;
	byte offset = 4096 % pitch;
	if (!abm_init_width(&bitmap, screen + offset, sizeof(screen) - offset, pitch, rows,
		BITMAP_MODE, BITMAP_PLAYFIELD) ||
	    !abm_display_list(display_list, sizeof(display_list), &bitmap, BITMAP_MODE, 24))
		return 1;

	byte ink = 1;
	os_color[0] = ATARI_COLOR(0, 14);
	os_color[1] = ATARI_COLOR(0, 14);
	os_color[2] = ATARI_COLOR(0, 0);
	os_color[4] = ATARI_COLOR(0, 0);
	os_gprior = 0;
#if BITMAP_MODE == ABM_GTIA
#if BITMAP_GTIA_MODE == 9
	os_gprior = PRIOR_GTIA_MODE9;
	ink = 14;
#elif BITMAP_GTIA_MODE == 10
	os_gprior = PRIOR_GTIA_MODE10;
	os_pcolr[0] = ATARI_COLOR(0, 0);
	os_pcolr[1] = ATARI_COLOR(0, 14);
#elif BITMAP_GTIA_MODE == 11
	os_gprior = PRIOR_GTIA_MODE11;
	os_color[4] = ATARI_COLOR(0, 8);
	ink = 10;
#else
#error Select GTIA mode 9, 10 or 11
#endif
#endif

	word quarter_width = bitmap.width / 4;
	byte quarter_height = rows / 4;
	abm_clear(&bitmap, 0);
	abm_fill_rect(&bitmap, quarter_width + 1, quarter_height + 1,
		quarter_width - 2, quarter_height - 2, ink);
	abm_fill_rect(&bitmap, quarter_width * 2 + 1, quarter_height * 3 + 1,
		quarter_width - 2, quarter_height - 2, ink);

	os_sdmctl = DMACTL_DLIST | BITMAP_PLAYFIELD;
	dl_set(display_list);
	for (;;)
		os_wait_frame();
}
