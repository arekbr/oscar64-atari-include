#include <atari/bitmap_indexed.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})
#pragma heapsize(0)
#pragma stacksize(1024)

static byte screen[8192];
#pragma align(screen, 4096)
static byte display_list[224];
#pragma align(display_list, 1024)

#if defined(DEMO_BMP) || defined(DEMO_PCX)
static byte work[ABM_INDEXED_CIO_WORK];
#else
static byte work[ABM_INDEXED_CIO_WORK + ABM_INDEXED_GIF_WORK];
#endif

int main(void)
{
	ABitmap bitmap;
#ifdef DEMO_WIDE
	byte mode = DL_MODEF;
#else
	byte mode = DL_MODEE;
#endif
	if (!abm_init(&bitmap, screen + 256, 7680, 40, 192, mode) ||
		!abm_display_list(display_list, sizeof(display_list), &bitmap, mode, 24))
		return 1;
	ABitmapIndexedImage image = {&bitmap, 0, work, sizeof(work)};
#ifdef DEMO_BMP
	byte result = abm_load_indexed(&image, ABM_INDEXED_BMP, "D:DEMO.BMP");
#elif defined(DEMO_PCX)
	byte result = abm_load_indexed(&image, ABM_INDEXED_PCX, "D:DEMO.PCX");
#else
	byte result = abm_load_indexed(&image, ABM_INDEXED_GIF, "D:DEMO.GIF");
#endif
	if (result != CIO_OK)
		return result;
	os_color[0] = 0x46;
	os_color[1] = 0x9a;
	os_color[2] = 0x0e;
	os_color[4] = 0;
	os_gprior &= 0x3f;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);
	for (;;)
		os_wait_frame();
}
