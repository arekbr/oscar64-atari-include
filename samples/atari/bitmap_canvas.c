#include <atari/bitmap_canvas.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>

#define PITCH 40
#define ROWS 96

static byte screen[PITCH * ROWS];
#pragma align(screen, 4096)
static byte display_list[104];
#pragma align(display_list, 1024)

static const byte hatch[8] = {0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11};

static void draw(const ABitmap * bitmap)
{
	ABitmapCanvas canvas;
	ABitmapRect clip = {12, 12, 136, 72};
	ABitmapRect background = {0, 0, 160, 96};
	ABitmapArc sector = {52, 45, 31, 27, 224, 160};
	ABitmapArc arc = {109, 46, 27, 23, 64, 192};
	ABitmapFont font = {(const byte *)0xe000, 0, 1};

	abm_clear(bitmap, 0);
	abm_canvas_init(&canvas, bitmap);
	abm_canvas_clip(&canvas, &clip);
	abm_canvas_brush(&canvas, 1, hatch);
	abm_canvas_fill(&canvas, &background);

	abm_canvas_brush(&canvas, 2, 0);
	abm_canvas_sector(&canvas, &sector);
	abm_canvas_pen(&canvas, 3, 0xffffu, 1);
	abm_canvas_arc(&canvas, &arc);
	abm_canvas_move(&canvas, 34, 73);
	abm_canvas_text(&canvas, "BITMAP CANVAS", &font);

	abm_canvas_pen(&canvas, 3, 0xc3c3u, 2);
	abm_canvas_line(&canvas, -20, 18, 180, 18);
}

int main(void)
{
	ABitmap bitmap;
	if (!abm_init(&bitmap, screen, sizeof(screen), PITCH, ROWS, DL_MODED) ||
		!abm_display_list(display_list, sizeof(display_list), &bitmap, DL_MODED, 24))
		return 1;
	draw(&bitmap);

	os_color[0] = ATARI_COLOR(8, 4);
	os_color[1] = ATARI_COLOR(3, 10);
	os_color[2] = ATARI_COLOR(0, 14);
	os_color[4] = ATARI_COLOR(0, 0);
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);
	for (;;)
		os_wait_frame();
}
