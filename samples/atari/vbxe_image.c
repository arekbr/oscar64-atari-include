#include <atari/os.h>
#include <atari/vbxe_image.h>

#pragma region(main, 0x2080, 0xa000, , , {code, data, bss, heap, stack})
#pragma heapsize(0)
#pragma stacksize(1024)

#define WIDTH 320
#define HEIGHT 192
#define PIXELS 0x10000ul

#if defined(DEMO_BMP)
#define FORMAT ABM_INDEXED_BMP
#define NAME "D:DEMO.BMP"
#define GIF_WORK 0
#elif defined(DEMO_PCX)
#define FORMAT ABM_INDEXED_PCX
#define NAME "D:DEMO.PCX"
#define GIF_WORK 0
#else
#define FORMAT ABM_INDEXED_GIF
#define NAME "D:DEMO.GIF"
#define GIF_WORK ABM_INDEXED_GIF_WORK
#endif

static byte work[VBXE_IMAGE_WORK + ABM_INDEXED_CIO_WORK + GIF_WORK];

int main(void)
{
	AVbxe board;
	if (!vbxe_detect(&board))
		return 1;
	if (!vbxe_fill(&board, PIXELS, 0, WIDTH * (word)HEIGHT))
		return 2;
	AVbxeImage image = {&board, PIXELS, WIDTH, WIDTH, HEIGHT, 1, work, sizeof(work)};
	byte status = vbxe_load_image(&image, FORMAT, NAME);
	if (status != CIO_OK)
		return status;
	static const AVbxeOverlay overlay = {0x08000ul, PIXELS, WIDTH, HEIGHT, 16, 1};
	if (!vbxe_show_indexed(&board, &overlay))
		return 3;
	for (;;)
		os_wait_frame();
}
