#include <atari/bitmap_curves.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#define STATUS (*(volatile byte *)0x0600)
#define PIXELS ((volatile byte *)0x8000)
#define HEIGHT 64

static volatile byte list[96];
#pragma align(list, 256)

static const ABitmapPoint quadratic[] = {{-6, 12}, {28, 3}, {62, 11}};
static const ABitmapPoint bezier[] = {{5, 25}, {18, 8}, {42, 33}, {61, 20}};
static const ABitmapPoint catmull[] = {{-8, 35}, {7, 38}, {29, 30}, {50, 41}, {69, 29}, {79, 37}};
static const ABitmapPoint spline[] = {{-4, 50}, {17, 54}, {17, 54}, {45, 45}, {71, 58}};

int main(void)
{
	ABitmap bitmap;
	STATUS = 0x11;
	if (!abm_init(&bitmap, PIXELS, 40 * HEIGHT, 40, HEIGHT, DL_MODEF))
		return 1;
	abm_clear(&bitmap, 0);
	ABitmapCanvas canvas;
	abm_canvas_init(&canvas, &bitmap);
	ABitmapRect clip = {5, 2, 65, 58};
	abm_canvas_clip(&canvas, &clip);
	abm_canvas_move(&canvas, 77, 81);
	if (abm_canvas_bezier(&canvas, bezier, 0) || abm_canvas_spline(&canvas, spline, 1, 5))
		return 2;
	STATUS = 0x21;
	if (!abm_canvas_quadratic(&canvas, quadratic, 7) || !abm_canvas_bezier(&canvas, bezier, 8))
		return 3;
	STATUS = 0x22;
	if (!abm_canvas_catmull_rom(&canvas, catmull, 6, 5) || !abm_canvas_spline(&canvas, spline, 5, 7))
		return 4;
	if (canvas.x != 77 || canvas.y != 81)
		return 5;
	if (!abm_display_list(list, sizeof(list), &bitmap, DL_MODEF, 16))
		return 6;
	os_color[1] = 0x0e;
	os_color[2] = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(list);
	STATUS = 0xa5;
	for (;;)
		;
}
