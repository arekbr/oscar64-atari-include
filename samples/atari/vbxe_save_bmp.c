#include <atari/vbxe_image.h>

#pragma region(main, 0x2800, 0x9800, , , {code, data, bss, heap, stack})

static AVbxePalette shadow;
static byte work[VBXE_IMAGE_MAX_WIDTH];
static const AVbxeColor colors[256] =
{
	{0,0,0}, {252,0,0}, {0,252,0}, {0,0,252}
};

int main(void)
{
	AVbxe board;
	if (!vbxe_detect(&board) || !vbxe_palette_track(&board, 1, &shadow) ||
		!vbxe_palette(&board, 1, 0, colors, 256))
		return 1;
	AVbxeImage image = {&board, 0x18ffdul, 320, 319, 192, 1, work, sizeof(work)};
	for (byte i = 0; i < 4; i++)
		if (!vbxe_fill(&board, image.pixels + (unsigned long)i * 15360u, i, 15360))
			return 2;
	byte status = vbxe_save_bmp(&image, 0, "D:CAPTURE.BMP");
	*(volatile byte *)0x0600 = status;
	return status == CIO_OK ? 0 : status;
}
