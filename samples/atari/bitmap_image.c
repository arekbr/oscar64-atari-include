#include <atari/bitmap_image.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})

static byte screen[8192];
#pragma align(screen, 4096)
static byte display_list[224];
#pragma align(display_list, 1024)
#ifndef IMAGE_CAPACITY
#define IMAGE_CAPACITY 8192
#endif
static byte file_data[IMAGE_CAPACITY];

int main(void)
{
	ABitmap bitmap;
	ABitmapPalette palette;
	if (!abm_init(&bitmap, screen + 256, 7680, 40, 192, DL_MODEE) ||
		!abm_display_list(display_list, sizeof(display_list), &bitmap, DL_MODEE, 24))
		return 1;
#ifdef DEMO_MIC
	const ABitmapImageFile file = {"D:DEMO.MIC", file_data, sizeof(file_data)};
	byte result = abm_load_mic(&bitmap, &file, &palette);
#else
	const ABitmapImageFile file = {"D:DEMO.PIC", file_data, sizeof(file_data)};
	byte result = abm_load_pic(&bitmap, &file, &palette);
#endif
	if (result != CIO_OK)
		return result;
	if (palette.count)
	{
		os_color[4] = palette.colors[0];
		for (byte i = 1; i < palette.count; i++)
			os_color[i - 1] = palette.colors[i];
	}
	os_gprior &= 0x3f;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);
	for (;;)
		os_wait_frame();
}
