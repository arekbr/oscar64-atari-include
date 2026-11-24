#include <atari/bitmap.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>

#define WIDTH 160
#define PITCH (WIDTH / 4)
#define ROWS  96

// ANTIC D has 160 four-color pixels per row, two scan lines per row.
// All 96 rows fit inside one aligned 4 KB screen page.
static byte screen[PITCH * ROWS];
#pragma align(screen, 4096)

static byte display_list[104];
#pragma align(display_list, 1024)

static byte flood_work[(WIDTH / 8) * ROWS];

static void draw_shapes(const ABitmap * bm)
{
	abm_clear(bm, 0);
	abm_text(bm, 24, 3, "BITMAP DRAWING", (const byte *)0xe000, 3);

	// A hollow ellipse and a solid ellipse share the same dimensions.
	abm_ellipse(bm, 40, 35, 26, 16, 1);
	abm_fill_ellipse(bm, 118, 35, 26, 16, 2);
	abm_ellipse(bm, 118, 35, 26, 16, 3);

	// Flood fill the background enclosed by a complete ellipse outline.
	abm_ellipse(bm, 40, 75, 27, 14, 3);
	if (!abm_flood_fill(bm, 40, 75, 2, flood_work, sizeof(flood_work)))
		abm_fill_rect(bm, 1, 92, 6, 3, 1);

	// Unset glyph bits preserve the colored rectangle beneath the text.
	abm_fill_rect(bm, 80, 64, 72, 24, 1);
	abm_text(bm, 88, 66, "OS ROM", (const byte *)0xe000, 3);
	abm_text(bm, 96, 78, "FONT", (const byte *)0xe000, 3);
}

int main(void)
{
	ABitmap bitmap;
	if (!abm_init(&bitmap, screen, sizeof(screen), PITCH, ROWS, DL_MODED) ||
	    !abm_display_list(display_list, sizeof(display_list), &bitmap, DL_MODED, 24) ||
	    abm_flood_size(&bitmap) > sizeof(flood_work))
		return 1;

	draw_shapes(&bitmap);

	os_color[0] = ATARI_COLOR(3, 8);
	os_color[1] = ATARI_COLOR(12, 8);
	os_color[2] = ATARI_COLOR(0, 14);
	os_color[4] = ATARI_COLOR(0, 0);
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);

	for (;;)
		os_wait_frame();
}
