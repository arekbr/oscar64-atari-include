#include <atari/os.h>
#include <atari/vbxe_canvas.h>

#pragma region(main, 0x2800, 0x9800, , , {code, data, bss, heap, stack})

#define STATUS (*(volatile byte *)0x0600)
#define PIXELS 0x21ffdul
#define HEIGHT 48
#define PITCH 352u

#ifndef DEMO_MODE
#define DEMO_MODE VBXE_DISPLAY_STANDARD
#endif

static const AVbxeColor palette[] = {
	{0,0,0}, {252,0,0}, {0,252,0}, {0,0,252}, {252,252,0}, {0,252,252},
	{252,0,252}, {252,252,252}, {140,140,140}
};

static const ABitmapPoint from = {-32768, -15991}, to = {32767, 16007};
static const ABitmapRect clip = {3, 2, 94, 44};
static const ABitmapRect painted = {-4, 4, 16, 6};
static const ABitmapRect outline = {15, 5, 10, 7};
static const ABitmapPoint polygon[] = {{-8, 18}, {13, 10}, {25, 20}, {14, 27}, {12, 20}, {5, 25}};
static const ABitmapPoint short_edge[] = {{30, 44}, {36, 49}};
static const ABitmapPoint crossed[] = {{61, 6}, {82, 19}, {60, 22}, {84, 3}};
static const ABitmapArc ellipse = {37, 13, 11, 7, 0, 256};
static const ABitmapArc arc = {50, 32, 11, 9, 230, 130};
static const ABitmapArc sector = {14, 39, 15, 10, 88, 198};
static const ABitmapRect bar = {75, 28, 19, 12};
static const ABitmapRect flood_clip = {67, 29, 16, 10};
static const ABitmapPoint seed = {70, 32};
static byte work[64];

static bool draw_shapes(const AVbxeCanvas * canvas)
{
	return vbxe_canvas_line(canvas, &from, &to, 5) &&
		vbxe_canvas_rect(canvas, &painted, 2, true) &&
		vbxe_canvas_rect(canvas, &outline, 3, false) &&
		vbxe_canvas_polygon(canvas, polygon, 6, 4, true) &&
		vbxe_canvas_polygon(canvas, crossed, 4, 8, true) &&
		vbxe_canvas_polygon(canvas, short_edge, 2, 7, false);
}

static bool draw_rounded(const AVbxeCanvas * canvas)
{
	return vbxe_canvas_arc(canvas, &ellipse, 6, false) &&
		vbxe_canvas_arc(canvas, &arc, 5, false) &&
		vbxe_canvas_arc(canvas, &sector, 3, true) &&
		vbxe_canvas_bar_3d(canvas, &bar, 7, 4, 7, true);
}

static bool seed_fill(AVbxeCanvas * canvas)
{
	const ABitmapRect wall = {72, 29, 1, 8};
	const ABitmapRect spot = {68, 34, 1, 1};
	const ABitmapPoint outside = {66, 32};
	vbxe_canvas_clip(canvas, &flood_clip);
	word size = vbxe_canvas_flood_size(canvas);
	if (size > sizeof(work) || !vbxe_canvas_rect(canvas, &wall, 1, true) ||
		!vbxe_canvas_rect(canvas, &spot, 8, true) ||
		vbxe_canvas_flood_fill(canvas, &seed, 8, work, size - 1) ||
		vbxe_canvas_flood_fill(canvas, &outside, 8, work, size))
		return false;
	return vbxe_canvas_flood_fill(canvas, &seed, 8, work, size) &&
		vbxe_canvas_flood_fill(canvas, &seed, 8, 0, 0);
}

int main(void)
{
	AVbxe board;
	STATUS = 0x11;
	if (!vbxe_detect(&board) || !vbxe_palette(&board, 2, 0, palette, 9) ||
		!vbxe_fill(&board, PIXELS - 1, 0xa7, PITCH * HEIGHT + 2))
		return 1;

	AVbxeDisplay display = {&board, 0x21000ul, PIXELS, 0, PITCH,
		DEMO_MODE, VBXE_DISPLAY_NORMAL, HEIGHT, 16, 2, 0, 255, VBXE_OPAQUE};
	AVbxeCanvas canvas;
	STATUS = 0x21;
	if (!vbxe_canvas_init(&canvas, &display) ||
		!vbxe_display_rect(&display, 0, 0, canvas.width - 1, HEIGHT - 1, 0))
		return 2;
	vbxe_canvas_clip(&canvas, &clip);
	STATUS = 0x31;
	if (!draw_shapes(&canvas))
		return 3;
	STATUS = 0x41;
	if (!draw_rounded(&canvas))
		return 4;
	STATUS = 0x51;
	if (!seed_fill(&canvas))
		return 5;

	STATUS = 0x61;
	if (!vbxe_display_show(&display))
		return 6;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
