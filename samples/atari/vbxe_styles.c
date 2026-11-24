#include <atari/os.h>
#include <atari/bitmap_canvas.h>
#include <atari/vbxe_display.h>
#include <atari/vbxe_terminal.h>

#pragma region(main, 0x2800, 0x9800, , , {code, data, bss, heap, stack})

#define STATUS (*(volatile byte *)0x0600)

static AVbxePalette shadow;
static byte glyphs[128 * 8];
static byte widths[128];
static const AVbxeColor rgb[8] =
{
	{0, 0, 0}, {254, 0, 0}, {0, 254, 0}, {0, 0, 254},
	{254, 254, 0}, {0, 254, 254}, {254, 0, 254}, {254, 254, 254}
};

#ifndef DEMO_TEXT
#define DEMO_TEXT 0
#endif
#ifndef DEMO_TEXT_ROWS
#define DEMO_TEXT_ROWS 32
#endif
#ifndef DEMO_MODE
#define DEMO_MODE VBXE_DISPLAY_STANDARD
#endif

static bool read_pixel(const AVbxeDisplay * display, word x, byte y, byte expected)
{
	volatile byte sample = 0xab;
	if (!vbxe_display_get(display, x, y, (byte *)&sample))
		return false;
	return sample == expected;
}

static bool bitmap(AVbxeDisplay * display)
{
	if (!vbxe_display_rect(display, 0, 0, vbxe_display_width(display) - 1, 31, 3) ||
		!vbxe_display_put(display, 1, 0, 4) || !read_pixel(display, 0, 0, 3) || !read_pixel(display, 1, 0, 4))
		return false;
	byte out = 0xab;
	if (vbxe_display_get(display, 0, 32, &out) || out != 0xab || vbxe_display_get(display, 0, 0, 0))
		return false;
	for (byte i = 0; i < 128; i++)
		widths[i] = 5;
	for (byte row = 0; row < 8; row++)
		glyphs[33 * 8 + row] = row == 7 ? 0xff : 0xa0;
	ABitmapFont font = {glyphs, widths, 1};
	AVbxeBitmapText text = {&font, -1, 2, 1, 2, VBXE_INK_OPAQUE | VBXE_INK_UNDERLINE | VBXE_INK_CONDENSED};
	if (!vbxe_display_text(display, &text, "AA") ||
		!read_pixel(display, 0, 2, 2) || !read_pixel(display, 1, 2, 1) ||
		!read_pixel(display, 4, 2, 3) || !read_pixel(display, 5, 2, 1) ||
		!read_pixel(display, 0, 8, 1) || !read_pixel(display, 1, 9, 3))
		return false;
	text.x = 1;
	text.y = 12;
	text.flags = VBXE_INK_INVERSE;
	if (!vbxe_display_text(display, &text, "A") ||
		!read_pixel(display, 1, 12, 3) || !read_pixel(display, 2, 12, 1) ||
		!read_pixel(display, 3, 12, 3) || !read_pixel(display, 4, 12, 1))
		return false;
	text.x = vbxe_display_width(display) - 1;
	text.y = -3;
	text.flags = 0;
	return vbxe_display_text(display, &text, "A") && read_pixel(display, text.x, 0, 1);
}

static AVbxeTerminal terminal;

#pragma optimize(push, 0, noinline, noconstparams)
static __noinline bool terminal_controls(const AVbxeDisplay * display)
{
	const char start[] = "\x1b[3";
#if DEMO_TEXT_ROWS == 30
	const char finish[] = "0;79H\x1b[1;31;44mZ\x1b[?25h";
#else
	const char finish[] = "2;79H\x1b[1;31;44mZ\x1b[?25h";
#endif
	const byte last = DEMO_TEXT_ROWS - 1;
	AVbxeTextCell cell;
	if (!vbxe_terminal_write(&terminal, start, sizeof(start) - 1) ||
		!vbxe_terminal_write(&terminal, finish, sizeof(finish) - 1) ||
		terminal.row != last || terminal.column != 79 || !terminal.shown ||
		!vbxe_display_read_text(display, 78, last, &cell, 1) || cell.glyph != 'Z' || cell.attribute != 0xc9)
		return false;
	if (!vbxe_terminal_cursor(&terminal, false) || !vbxe_terminal_scroll(&terminal, last - 2, last, 1))
		return false;
	return vbxe_display_read_text(display, 78, last - 1, &cell, 1) && cell.glyph == 'Z' &&
		vbxe_display_read_text(display, 78, last, &cell, 1) && cell.glyph == ' ';
}

#pragma optimize(pop)

static bool text_screen(AVbxeDisplay * display)
{
	AVbxeTextCell cells[80];
	byte stripes[16] = {255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255, 0};
	if (!vbxe_write(display->board, display->charset + 8, stripes, sizeof(stripes)) ||
		!vbxe_terminal_init(&terminal, display, 0) || terminal.rows != DEMO_TEXT_ROWS || !terminal_controls(display))
		return false;
	for (byte row = 0; row < DEMO_TEXT_ROWS; row++)
	{
		for (byte col = 0; col < 80; col++)
		{
			cells[col].glyph = 1;
			cells[col].attribute = 1 + (row & 1);
		}
		if (!vbxe_display_write_text(display, 0, row, cells, 80))
			return false;
	}
	AVbxeTextCell last = {2, 5};
	AVbxeTextCell out;
	return vbxe_display_write_text(display, 79, DEMO_TEXT_ROWS - 1, &last, 1) &&
		vbxe_display_read_text(display, 79, DEMO_TEXT_ROWS - 1, &out, 1) && out.glyph == 2 && out.attribute == 5 &&
		!vbxe_display_write_text(display, 0, DEMO_TEXT_ROWS, &last, 1);
}

int main(void)
{
	AVbxe board;
	STATUS = 0x11;
	if (!vbxe_detect(&board) || !vbxe_palette_track(&board, 1, &shadow) ||
		vbxe_palette_entries(&board, 1, 0, 1) || !vbxe_palette(&board, 1, 0, rgb, 8) ||
		vbxe_palette_entries(&board, 1, 0, 9) || vbxe_palette_entries(&board, 2, 0, 1))
		return 1;
	const AVbxeColor * readback = vbxe_palette_entries(&board, 1, 1, 7);
	if (!readback || readback[6].red != 254 || readback[4].green != 254 || readback[0].blue)
		return 2;
	byte text_mode = DEMO_TEXT_ROWS == 30 ? VBXE_DISPLAY_TEXT_80X30 : VBXE_DISPLAY_TEXT_80X32;
	AVbxeDisplay display = {&board, 0x10000ul, 0x12000ul, 0x10800ul, 336,
		DEMO_TEXT ? text_mode : DEMO_MODE, VBXE_DISPLAY_NORMAL,
		DEMO_TEXT ? DEMO_TEXT_ROWS * 7 : 32, 8, 1, 0, 255, VBXE_OPAQUE};
	STATUS = 0x22;
	if (DEMO_TEXT ? !text_screen(&display) : !bitmap(&display))
		return 3;
	STATUS = 0x33;
	if (!vbxe_display_show(&display))
		return 4;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
