#include "vbxe_terminal.h"

enum ParserState {GROUND, ESCAPE, ESCAPE_EXTRA, CSI, DISCARD_CSI, STRING, STRING_ESCAPE};

static byte glyph(const AVbxeTerminal * terminal, byte ascii)
{
	return terminal->glyphs ? terminal->glyphs[ascii] : ascii;
}

static byte attribute(const AVbxeTerminal * terminal)
{
	byte fg = terminal->foreground, bg = terminal->background;
	if (terminal->inverse)
		return 0x80 | ((fg & 7) << 4) | bg;
	return 0x80 | (bg << 4) | fg;
}

static void reset(AVbxeTerminal * terminal)
{
	terminal->column = terminal->row = terminal->top = 0;
	terminal->saved_column = terminal->saved_row = 0;
	terminal->bottom = terminal->rows - 1;
	terminal->foreground = terminal->saved_foreground = 7;
	terminal->background = terminal->saved_background = 0;
	terminal->inverse = terminal->saved_inverse = false;
	terminal->wrap = true;
	terminal->pending = terminal->cursor = terminal->shown = false;
	terminal->scrollback_ready = false;
	terminal->state = GROUND;
}

bool vbxe_terminal_init(AVbxeTerminal * terminal, const AVbxeDisplay * display, const byte * glyphs)
{
	if (!terminal || !vbxe_display_write_text(display, 0, 0, 0, 0) || display->palette >= 4)
		return false;
	terminal->display = display;
	terminal->glyphs = glyphs;
	terminal->columns = vbxe_display_width(display);
	terminal->rows = vbxe_display_rows(display);
	terminal->scrollback = (AVbxeTextCell *)0;
	reset(terminal);
	return true;
}

bool vbxe_terminal_palette(const AVbxeTerminal * terminal)
{
	static const AVbxeColor rgb[16] =
	{
		{0,0,0}, {170,0,0}, {0,170,0}, {170,85,0},
		{0,0,170}, {170,0,170}, {0,170,170}, {170,170,170},
		{85,85,85}, {255,85,85}, {85,255,85}, {255,255,85},
		{85,85,255}, {255,85,255}, {85,255,255}, {255,255,255}
	};
	if (!terminal || !terminal->display)
		return false;
	const AVbxeDisplay * display = terminal->display;
	AVbxeColor block[16];
	for (byte group = 0; group < 8; group++)
	{
		byte first = group << 4;
		if (!vbxe_palette(display->board, display->palette, first, rgb, 16))
			return false;
		for (byte i = 0; i < 16; i++)
			block[i] = rgb[group];
		if (!vbxe_palette(display->board, display->palette, first | 0x80, block, 16))
			return false;
	}
	return true;
}

static bool hide_cursor(AVbxeTerminal * terminal)
{
	if (!terminal->shown)
		return true;
	if (!vbxe_display_write_text(terminal->display, terminal->column, terminal->row, &terminal->saved_cell, 1))
		return false;
	terminal->shown = false;
	return true;
}

static bool show_cursor(AVbxeTerminal * terminal)
{
	if (!terminal->cursor)
		return true;
	if (!vbxe_display_read_text(terminal->display, terminal->column, terminal->row, &terminal->saved_cell, 1))
		return false;
	AVbxeTextCell cell = terminal->saved_cell;
	// Use contrasting foreground/background pairs, including blank cells.
	cell.attribute = (cell.attribute & 0x70) == 0x70 ? 0x87 : 0xf0;
	if (!vbxe_display_write_text(terminal->display, terminal->column, terminal->row, &cell, 1))
		return false;
	terminal->shown = true;
	return true;
}

bool vbxe_terminal_cursor(AVbxeTerminal * terminal, bool visible)
{
	if (!terminal || !terminal->display || !hide_cursor(terminal))
		return false;
	terminal->cursor = visible;
	return show_cursor(terminal);
}

#pragma optimize(push, 0, noinline, noconstparams)
static __noinline bool erase(AVbxeTerminal * request, byte first, byte last, byte column, byte count)
{
	AVbxeTerminal * volatile terminal = request;
	volatile byte length = count;
	const byte * glyphs = terminal->glyphs;
	byte space = glyphs ? glyphs[' '] : ' ';
	byte attr = attribute(terminal);
	for (byte i = 0; i < length; i++)
	{
		terminal->work[i].glyph = space;
		terminal->work[i].attribute = attr;
	}
	for (byte row = first; row <= last; row++)
		if (!vbxe_display_write_text(terminal->display, column, row, terminal->work, length))
			return false;
	return true;
}
#pragma optimize(pop)

static bool clear(AVbxeTerminal * terminal)
{
	return erase(terminal, 0, terminal->rows - 1, 0, terminal->columns);
}

bool vbxe_terminal_clear(AVbxeTerminal * terminal)
{
	return terminal && terminal->display && hide_cursor(terminal) && clear(terminal) && show_cursor(terminal);
}

// Keep the bind-time linker address checks in one runtime function.
#pragma optimize(push, 0, noinline, noconstparams)
bool vbxe_terminal_scrollback_bind(AVbxeTerminal * terminal, AVbxeTextCell * cells, byte capacity)
{
	if (!terminal || !terminal->display)
		return false;
	if (cells)
	{
		word start = (word)cells;
		word size = (word)terminal->columns * sizeof(AVbxeTextCell);
		if (capacity < terminal->columns || start > 0xffffu - (size - 1))
			return false;
		word end = start + (size - 1);
		word owner = (word)terminal;
		if ((end >= VBXE_MEMAC_WINDOW && start < VBXE_MEMAC_WINDOW + VBXE_MEMAC_SIZE) ||
			(end >= owner && start <= owner + (sizeof(AVbxeTerminal) - 1)))
			return false;
	}
	terminal->scrollback = cells;
	terminal->scrollback_ready = false;
	return true;
}
#pragma optimize(pop)

bool vbxe_terminal_scrollback_take(AVbxeTerminal * terminal)
{
	if (!terminal || !terminal->scrollback_ready)
		return false;
	terminal->scrollback_ready = false;
	return true;
}

static __noinline bool scroll_rows(AVbxeTerminal * terminal, byte first, byte last, int amount)
{
	byte height = last - first + 1;
	if (amount >= height || amount <= -(int)height)
		return erase(terminal, first, last, 0, terminal->columns);
	bool up = amount > 0;
	byte count = up ? amount : -amount;
	for (byte i = 0; i < height - count; i++)
	{
		byte dest = up ? first + i : last - i;
		byte source = up ? dest + count : dest - count;
		if (!vbxe_display_read_text(terminal->display, 0, source, terminal->work, terminal->columns) ||
			!vbxe_display_write_text(terminal->display, 0, dest, terminal->work, terminal->columns))
			return false;
	}
	if (up)
		first = last - count + 1;
	else
		last = first + count - 1;
	return erase(terminal, first, last, 0, terminal->columns);
}

static __noinline bool scroll(AVbxeTerminal * terminal, byte first, byte last, int amount)
{
	if (!amount)
		return true;
	if (terminal->scrollback)
	{
		terminal->scrollback_ready = false;
		byte outgoing = amount > 0 ? first : last;
		if (!vbxe_display_read_text(terminal->display, 0, outgoing, terminal->scrollback, terminal->columns))
			return false;
	}
	if (!scroll_rows(terminal, first, last, amount))
		return false;
	terminal->scrollback_ready = terminal->scrollback != 0;
	return true;
}

bool vbxe_terminal_scroll(AVbxeTerminal * terminal, byte first, byte last, int amount)
{
	return terminal && terminal->display && first <= last && last < terminal->rows &&
		hide_cursor(terminal) && scroll(terminal, first, last, amount) && show_cursor(terminal);
}

static bool index_line(AVbxeTerminal * terminal, bool up)
{
	terminal->pending = false;
	if (up)
	{
		if (terminal->row == terminal->bottom)
			return scroll(terminal, terminal->top, terminal->bottom, 1);
		if (terminal->row + 1 < terminal->rows)
			terminal->row++;
	}
	else
	{
		if (terminal->row == terminal->top)
			return scroll(terminal, terminal->top, terminal->bottom, -1);
		if (terminal->row)
			terminal->row--;
	}
	return true;
}

static byte move(byte origin, int distance, byte limit)
{
	int to = origin + distance;
	return to < 0 ? 0 : to >= limit ? limit - 1 : to;
}

static byte parameter(const AVbxeTerminal * terminal, byte index, byte fallback)
{
	return index <= terminal->argument && terminal->params[index] ? terminal->params[index] : fallback;
}

static void save_cursor(AVbxeTerminal * terminal)
{
	terminal->saved_column = terminal->column;
	terminal->saved_row = terminal->row;
	terminal->saved_foreground = terminal->foreground;
	terminal->saved_background = terminal->background;
	terminal->saved_inverse = terminal->inverse;
}

static void restore_cursor(AVbxeTerminal * terminal)
{
	terminal->column = terminal->saved_column;
	terminal->row = terminal->saved_row;
	terminal->foreground = terminal->saved_foreground;
	terminal->background = terminal->saved_background;
	terminal->inverse = terminal->saved_inverse;
	terminal->pending = false;
}

static __noinline void sgr(AVbxeTerminal * terminal)
{
	for (byte i = 0; i <= terminal->argument; i++)
	{
		byte n = terminal->params[i];
		if (!n)
		{
			terminal->foreground = 7;
			terminal->background = 0;
			terminal->inverse = false;
		}
		else if (n == 1)
			terminal->foreground |= 8;
		else if (n == 22)
			terminal->foreground &= 7;
		else if (n == 7 || n == 27)
			terminal->inverse = n == 7;
		else if (n >= 30 && n <= 37)
			terminal->foreground = (terminal->foreground & 8) | (n - 30);
		else if (n >= 90 && n <= 97)
			terminal->foreground = n - 82;
		else if (n == 39)
			terminal->foreground = 7;
		else if (n >= 40 && n <= 47)
			terminal->background = n - 40;
		else if (n >= 100 && n <= 107)
			terminal->background = n - 100;
		else if (n == 49)
			terminal->background = 0;
		else if (n == 38 || n == 48 || n == 58)
		{
			// Consume extended-color operands as a group.
			if (i == terminal->argument)
				return;
			byte model = terminal->params[++i];
			byte fields = model == 2 ? 3 : model == 5 ? 1 : 0;
			if (!fields || fields > terminal->argument - i)
				return;
			i += fields;
		}
	}
}

static bool erase_display(AVbxeTerminal * terminal, byte mode)
{
	byte row = terminal->row, column = terminal->column;
	if (mode == 0)
		return erase(terminal, row, row, column, terminal->columns - column) &&
			(row + 1 == terminal->rows || erase(terminal, row + 1, terminal->rows - 1, 0, terminal->columns));
	if (mode == 1)
		return (!row || erase(terminal, 0, row - 1, 0, terminal->columns)) &&
			erase(terminal, row, row, 0, column + 1);
	return mode != 2 || clear(terminal);
}

static __noinline bool dispatch(AVbxeTerminal * terminal, byte cmd)
{
	byte one = parameter(terminal, 0, 1);
	int amount = one;
	if (terminal->private_csi)
	{
		if (cmd == 'h' || cmd == 'l')
		{
			for (byte i = 0; i <= terminal->argument; i++)
			{
				if (terminal->params[i] == 7)
				{
					terminal->wrap = cmd == 'h';
					if (!terminal->wrap)
						terminal->pending = false;
				}
				if (terminal->params[i] == 25)
					terminal->cursor = cmd == 'h';
			}
		}
		return true;
	}
	if (cmd == 'm')
	{
		sgr(terminal);
		return true;
	}
	terminal->pending = false;
	byte row = terminal->row, col = terminal->column;
	switch (cmd)
	{
	case 'A': terminal->row = move(row, -amount, terminal->rows); break;
	case 'E': terminal->column = 0;
	case 'B': terminal->row = move(row, amount, terminal->rows); break;
	case 'C': terminal->column = move(col, amount, terminal->columns); break;
	case 'D': terminal->column = move(col, -amount, terminal->columns); break;
	case 'F': terminal->row = move(row, -amount, terminal->rows); terminal->column = 0; break;
	case 'G': terminal->column = move(0, amount - 1, terminal->columns); break;
	case 'd': terminal->row = move(0, amount - 1, terminal->rows); break;
	case 'H':
	case 'f':
		terminal->row = move(0, amount - 1, terminal->rows);
		terminal->column = move(0, parameter(terminal, 1, 1) - 1, terminal->columns);
		break;
	case 'J': return erase_display(terminal, terminal->params[0]);
	case 'K':
		switch (terminal->params[0])
		{
		case 0: return erase(terminal, row, row, col, terminal->columns - col);
		case 1: return erase(terminal, row, row, 0, col + 1);
		case 2: return erase(terminal, row, row, 0, terminal->columns);
		}
		break;
	case 'S': return scroll(terminal, terminal->top, terminal->bottom, amount);
	case 'T': return scroll(terminal, terminal->top, terminal->bottom, -amount);
	case 'L':
	case 'M':
		if (row >= terminal->top && row <= terminal->bottom)
			return scroll(terminal, row, terminal->bottom, cmd == 'M' ? amount : -amount);
		break;
	case 'r':
	{
		byte last = parameter(terminal, 1, terminal->rows);
		if (one < last && last <= terminal->rows)
		{
			terminal->top = one - 1;
			terminal->bottom = last - 1;
			terminal->column = terminal->row = 0;
		}
		break;
	}
	case 's': save_cursor(terminal); break;
	case 'u': restore_cursor(terminal); break;
	}
	return true;
}

static void start_csi(AVbxeTerminal * terminal)
{
	terminal->state = CSI;
	terminal->argument = terminal->params[0] = 0;
	terminal->private_csi = false;
}

static bool csi(AVbxeTerminal * terminal, byte value)
{
	if (value >= 0x40 && value <= 0x7e)
	{
		bool valid = terminal->state == CSI;
		terminal->state = GROUND;
		return !valid || dispatch(terminal, value);
	}
	if (terminal->state == DISCARD_CSI)
		return true;
	byte i = terminal->argument;
	if (value >= '0' && value <= '9')
	{
		word number = 10u * terminal->params[i] + value - '0';
		terminal->params[i] = number > 255 ? 255 : number;
	}
	else if (value == ';' && i < sizeof(terminal->params) - 1)
	{
		i++;
		terminal->argument = i;
		terminal->params[i] = 0;
	}
	else if (value == '?' && !i && !terminal->params[0] && !terminal->private_csi)
		terminal->private_csi = true;
	else
		terminal->state = DISCARD_CSI;
	return true;
}

static __noinline bool consume(AVbxeTerminal * terminal, byte value)
{
	byte state = terminal->state;
	if (value == 0x18 || value == 0x1a)
	{
		terminal->state = GROUND;
		return true;
	}
	if (state == STRING || state == STRING_ESCAPE)
	{
		if (value == 7 || value == 0x9c || (state == STRING_ESCAPE && value == '\\'))
			terminal->state = GROUND;
		else
			terminal->state = value == 27 ? STRING_ESCAPE : STRING;
		return true;
	}
	if (value == 27)
	{
		terminal->state = ESCAPE;
		return true;
	}
	if (value == 0x9b)
	{
		start_csi(terminal);
		return true;
	}
	switch (value)
	{
	case 8: terminal->column = move(terminal->column, -1, terminal->columns); terminal->pending = false; return true;
	case 9: terminal->column = move(terminal->column & ~7, 8, terminal->columns); terminal->pending = false; return true;
	case 10:
	case 11:
	case 12: return index_line(terminal, true);
	case 13: terminal->column = 0; terminal->pending = false; return true;
	}
	if (value < 32 || value == 127)
		return true;
	if (state == CSI || state == DISCARD_CSI)
		return csi(terminal, value);
	if (state == ESCAPE_EXTRA)
	{
		if (value >= 0x30)
			terminal->state = GROUND;
		return true;
	}
	if (state == ESCAPE)
	{
		terminal->state = GROUND;
		switch (value)
		{
		case '[': start_csi(terminal); break;
		case ']':
		case 'P':
		case '^':
		case '_': terminal->state = STRING; break;
		case 'c': reset(terminal); return clear(terminal);
		case '7': save_cursor(terminal); break;
		case '8': restore_cursor(terminal); break;
		case 'E': terminal->column = 0; return index_line(terminal, true);
		case 'D': return index_line(terminal, true);
		case 'M': return index_line(terminal, false);
		default:
			if (value < 0x30)
				terminal->state = ESCAPE_EXTRA;
			break;
		}
		return true;
	}
	if (value >= 128)
		return true;
	if (terminal->pending)
	{
		terminal->column = 0;
		if (!index_line(terminal, true))
			return false;
	}
	AVbxeTextCell cell = {glyph(terminal, value), attribute(terminal)};
	if (!vbxe_display_write_text(terminal->display, terminal->column, terminal->row, &cell, 1))
		return false;
	if (terminal->column + 1 < terminal->columns)
		terminal->column++;
	else
		terminal->pending = terminal->wrap;
	return true;
}

#pragma optimize(push, 2, noinline, noconstparams)
bool vbxe_terminal_write(AVbxeTerminal * terminal, const char * data, word count)
{
	if (!terminal || !terminal->display || (count && !data) || !hide_cursor(terminal))
		return false;
	while (count--)
		if (!consume(terminal, (byte)*data++))
			return false;
	return show_cursor(terminal);
}
#pragma optimize(pop)
