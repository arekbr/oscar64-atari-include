#include <atari/bitmap.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>

#define PITCH       40
#define HALF_LINES  96
#define HALF_BYTES  (PITCH * HALF_LINES)
#define PAGE_BYTES  4096

#ifdef BITMAP_HIRES
#define MODE        DL_MODEF
#define X_SCALE     2
#define UPPER_BG    0
#define UPPER_INK   1
#define LOWER_BG    1
#else
#define MODE        DL_MODEE
#define X_SCALE     1
#define UPPER_BG    1
#define UPPER_INK   3
#define LOWER_BG    2
#endif

static byte screen[2 * PAGE_BYTES];
#pragma align(screen, 4096)

static byte display_list[208];
#pragma align(display_list, 1024)

static void draw_upper(const ABitmap * bm)
{
	abm_clear(bm, UPPER_BG);
	abm_line(bm, 40 * X_SCALE, 20, 119 * X_SCALE, 20, UPPER_INK);
	abm_line(bm, 119 * X_SCALE, 20, 119 * X_SCALE, 74, UPPER_INK);
	abm_line(bm, 119 * X_SCALE, 74, 40 * X_SCALE, 74, UPPER_INK);
	abm_line(bm, 40 * X_SCALE, 74, 40 * X_SCALE, 20, UPPER_INK);
	abm_put(bm, 80 * X_SCALE, 47, UPPER_INK);
}

static void draw_lower(const ABitmap * bm)
{
	abm_clear(bm, LOWER_BG);
	abm_line(bm, 32 * X_SCALE, 47, 127 * X_SCALE, 47, 0);
	abm_line(bm, 80 * X_SCALE, 16, 80 * X_SCALE, 78, 0);
}

int main(void)
{
	ABitmap upper, lower;
	volatile byte * dl = display_list;

	if (!abm_init(&upper, screen, HALF_BYTES, PITCH, HALF_LINES, MODE) ||
	    !abm_init(&lower, screen + PAGE_BYTES, HALF_BYTES, PITCH, HALF_LINES, MODE))
		return 1;

	draw_upper(&upper);
	draw_lower(&lower);

	dl = dl_blank(dl, 24);
	dl = dl_lms(dl, MODE, screen);
	dl = dl_mode(dl, MODE, HALF_LINES - 1);
	// Reload ANTIC's screen address at the start of the second 4 KB page.
	dl = dl_lms(dl, MODE, screen + PAGE_BYTES);
	dl = dl_mode(dl, MODE, HALF_LINES - 1);
	dl_jvb(dl, display_list);

#ifdef BITMAP_HIRES
	os_color[1] = ATARI_COLOR(0, 14);  // Set pixels use PF1 luminance.
	os_color[2] = ATARI_COLOR(0, 0);   // PF2 gives the hue and clear-pixel color.
#else
	os_color[0] = ATARI_COLOR(3, 8);
	os_color[1] = ATARI_COLOR(12, 8);
	os_color[2] = ATARI_COLOR(0, 14);
#endif
	os_color[4] = ATARI_COLOR(0, 0);
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);

	for (;;)
		os_wait_frame();
}
