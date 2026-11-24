#include <atari/os.h>
#include <atari/vbxe_display.h>

#pragma region(main, 0x2800, 0x9800, , , {code, data, bss, heap, stack})

static const AVbxeColor first[2] = {{0,0,0}, {252,0,0}};
static const AVbxeColor second[2] = {{0,0,0}, {0,252,0}};

int main(void)
{
	AVbxe board;
	if (!vbxe_detect(&board))
		return 1;
	AVbxeDisplay display = {&board, 0x10000ul, 0x11000ul, 0, 320,
		VBXE_DISPLAY_STANDARD, VBXE_DISPLAY_NORMAL, 32, 40, 1, 0, 255, VBXE_OPAQUE};
	const AVbxeColorMap map = {0x13800ul, 80, 16, 8, 0, 0};
	const AVbxeDisplayCopy copy = {0x14000ul, 32, 36, 28, 8, 11, 16, false};
	if (!vbxe_palette(&board, 1, 0, first, 2) || !vbxe_palette(&board, 2, 0, second, 2) ||
		!vbxe_display_rect(&display, 0, 0, 319, 31, 0) ||
		!vbxe_display_rect(&display, 32, 8, 59, 23, 1))
		return 2;
	for (byte y = 0; y < 4; y++)
	{
		byte cells[80];
		for (byte x = 0; x < 20; x++)
		{
			word i = (word)x << 2;
			cells[i] = cells[i + 1] = cells[i + 2] = 0;
			cells[i + 3] = ((x ^ y) & 1) ? 0x10 : 0x20;
		}
		if (!vbxe_write(&board, map.data + (word)y * 80, cells, sizeof(cells)))
			return 3;
	}
	if (!vbxe_display_map_priority(&board, 0, 255) || !vbxe_display_copy(&display, &copy))
		return 4;
	while (vbxe_blitter_busy(&board))
		;
	if (!vbxe_display_show_map(&display, &map))
		return 5;
	for (;;)
		os_wait_frame();
}
