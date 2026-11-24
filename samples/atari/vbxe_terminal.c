#include <atari/os.h>
#include <atari/vbxe_terminal.h>

#pragma region(main, 0x2800, 0x9800, , , {code, data, bss, heap, stack})

static AVbxeTerminal terminal;
static byte glyphs[128];
static const char title[] =
	"\x1b[2J\x1b[2;4H\x1b[1;33;44m VideoBoard XE ANSI \x1b[0m"
	"\x1b[4;4HABCDEFGH\r\n   \x1b[31mred\x1b[32m green\x1b[36m cyan\x1b[0m"
	"\x1b[8;4H\x1b[?25h";

int main(void)
{
	AVbxe board;
	if (!vbxe_detect(&board))
		return 1;
	AVbxeDisplay display = {&board, 0x10000ul, 0x12000ul, 0x10800ul, 160,
		VBXE_DISPLAY_TEXT, VBXE_DISPLAY_NORMAL, 192, 16, 1, 0, 255, VBXE_OPAQUE};
	const byte * font = (const byte *)((word)os_chbas << 8);
	for (byte i = 32; i < 127; i++)
		glyphs[i] = i < 96 ? i - 32 : i;
	if (!vbxe_write(&board, display.charset, font, 1024) ||
		!vbxe_terminal_init(&terminal, &display, glyphs) ||
		!vbxe_terminal_palette(&terminal) ||
		!vbxe_terminal_write(&terminal, title, sizeof(title) - 1) ||
		!vbxe_display_show(&display))
		return 2;
	for (;;)
		os_wait_frame();
}
