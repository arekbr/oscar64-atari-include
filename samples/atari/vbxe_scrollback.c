#include <atari/os.h>
#include <atari/vbxe_terminal.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#ifndef DEMO_ROWS
#define DEMO_ROWS 24
#endif

#define STATUS (*(volatile byte *)0x0600)
#define CELLS ((AVbxeTextCell *)0x8000)
#define VRAM_TEXT 0x12fa0ul
#define COLUMNS 80
#define PITCH 192

static AVbxeTerminal terminal;
static AVbxeTextCell row[COLUMNS];

static byte style(byte number, byte column)
{
	return 0x80 | (column & 1 ? 0x10 : 0) | number;
}

static bool set_row(const AVbxeDisplay * display, byte line, byte number)
{
	for (byte x = 0; x < COLUMNS; x++)
	{
		row[x].glyph = number;
		row[x].attribute = style(number, x);
	}
	return vbxe_display_write_text(display, 0, line, row, COLUMNS);
}

static bool captured(byte number)
{
	if (!vbxe_terminal_scrollback_take(&terminal) || vbxe_terminal_scrollback_take(&terminal))
		return false;
	for (byte x = 0; x < COLUMNS; x++)
		if (CELLS[x].glyph != number || CELLS[x].attribute != style(number, x))
			return false;
	return true;
}

static bool upward(const AVbxeDisplay * display)
{
	static const char cursor[] = "\x1b[2;3H\x1b[?25h";
	if (!vbxe_terminal_write(&terminal, cursor, sizeof(cursor) - 1) || !terminal.shown ||
		!vbxe_terminal_scroll(&terminal, 1, 4, 1) || !terminal.shown ||
		!vbxe_terminal_scroll(&terminal, 1, 4, 0) || !captured(2))
		return false;
	if (!vbxe_terminal_cursor(&terminal, false) ||
		!vbxe_display_read_text(display, 0, 1, row, COLUMNS))
		return false;
	for (byte x = 0; x < COLUMNS; x++)
		if (row[x].glyph != 3 || row[x].attribute != style(3, x))
			return false;
	return true;
}

static bool shifting(const AVbxeDisplay * display)
{
	static const char reverse[] = "\x1b[2;5r\x1b[2;1H\x1bM";
	static const char newline[] = "\x1b[5;1H\n";
	if (!set_row(display, 4, 5) || !vbxe_terminal_write(&terminal, reverse, sizeof(reverse) - 1) ||
		!captured(5) || !set_row(display, 1, 2) ||
		!vbxe_terminal_write(&terminal, newline, sizeof(newline) - 1) || !captured(2))
		return false;
	return set_row(display, 4, 4) && vbxe_terminal_scroll(&terminal, 2, 4, -32768) && captured(4);
}

static bool final_frame(const AVbxeDisplay * display)
{
	// Render the saved top row separately from the new visible top row.
	if (!set_row(display, 0, 1) || !set_row(display, 1, 6) ||
		!vbxe_terminal_scroll(&terminal, 0, 1, 1) ||
		!vbxe_terminal_scrollback_take(&terminal) ||
		!vbxe_display_write_text(display, 0, 1, CELLS, COLUMNS))
		return false;
	if (!vbxe_terminal_scrollback_bind(&terminal, 0, 0) ||
		!vbxe_terminal_scroll(&terminal, 4, 5, -1) ||
		vbxe_terminal_scrollback_take(&terminal))
		return false;
	return vbxe_display_show(display);
}

int main(void)
{
	STATUS = 1;
	AVbxe board;
	if (!vbxe_detect(&board))
		return 1;
	AVbxeDisplay display = {&board, 0x10000ul, VRAM_TEXT, 0x10800ul, PITCH,
#if DEMO_ROWS == 32
		VBXE_DISPLAY_TEXT_80X32, VBXE_DISPLAY_NORMAL, 224, 8,
#else
		VBXE_DISPLAY_TEXT, VBXE_DISPLAY_NORMAL, 192, 16,
#endif
		1, 0, 255, VBXE_OPAQUE};
	if (!vbxe_terminal_init(&terminal, &display, 0) || terminal.rows != DEMO_ROWS ||
		!vbxe_terminal_palette(&terminal) || !vbxe_fill(&board, display.charset, 0, 2048) ||
		!vbxe_fill(&board, display.charset + 8, 0xff, 48) ||
		!vbxe_fill(&board, VRAM_TEXT - 1, 0xa6, PITCH * DEMO_ROWS + 2) || !vbxe_terminal_clear(&terminal))
		return 2;
	STATUS = 2;
	if (vbxe_terminal_scrollback_bind(&terminal, CELLS, COLUMNS - 1) ||
		vbxe_terminal_scrollback_bind(&terminal, terminal.work, 84) ||
		vbxe_terminal_scrollback_bind(&terminal, (AVbxeTextCell *)0x9fff, COLUMNS) ||
		!vbxe_terminal_scrollback_bind(&terminal, CELLS, COLUMNS) ||
		vbxe_terminal_scrollback_take(&terminal))
		return 3;
	for (byte i = 0; i < 6; i++)
		if (!set_row(&display, i, i + 1))
			return 4;
	STATUS = 3;
	if (!upward(&display))
		return 5;
	STATUS = 4;
	if (!shifting(&display))
		return 6;
	STATUS = 5;
	if (!final_frame(&display))
		return 7;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
