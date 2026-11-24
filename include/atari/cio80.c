#include "cio80.h"
#include "os.h"

#define HANDLERS ((volatile byte *)0x031a)
#define ZIOCB (*(volatile IOCB *)0x0020)
#define BRKKEY (*(volatile byte *)0x0011)
#define OLDROW (*(volatile byte *)0x005a)
#define OLDCOL (*(volatile word *)0x005b)
#define SSFLAG (*(volatile byte *)0x02ff)

static const ABitmapDisplayPage * display;
static const byte * font_data;
static byte letters[24][80];
static byte register_slot, channel, mode, input_mode, in_pos, in_end;
static bool installed, escaped, input_pending;
static volatile bool opened; // Changed through the CIO dispatch vector.
static word old_list, old_screen, old_col, other_col, draw_x;
static byte old_dma, old_priority, old_colors[3], old_left, old_right, old_row, old_mode, other_row, draw_y;

static volatile byte entry, entry_a, entry_x, reply;
static volatile byte font_character;
static const byte atascii_xor[] = {0x40, 0x20, 0x60, 0};
// Oscar64 reserves $80–$DF for its registers and temporaries on Atari.
// $E0–$FE can hold live globals, including this handler's own state.
static volatile byte saved_zp[96];
static volatile byte vector_table[15];

static __noinline byte font_index(byte ch)
{
	// A runtime table lookup is required for bytes from every ATASCII group.
	font_character = ch;
	return __asm {
		lda font_character
		and #0x60
		lsr
		lsr
		lsr
		lsr
		lsr
		tax
		lda font_character
		and #0x7f
		eor atascii_xor,x
		sta accu
	};
}

static const byte * cell_font(byte ch)
{
	return font_data + (word)font_index(ch) * 8;
}

static byte set_cell(byte x, byte y, byte ch)
{
	const byte * glyph = cell_font(ch);
	byte inverse = ch & 128 ? 15 : 0;
	byte shift = x & 1 ? 0 : 4;
	byte keep = ~(15 << shift);
	volatile byte * pixels = abm_row(&display->bitmap, y << 3) + (x >> 1);
	for (byte i = 0; i < 8; i++)
	{
		byte ink = (glyph[i] & 15) ^ inverse;
		*pixels = (*pixels & keep) | (ink << shift);
		pixels += display->bitmap.pitch;
	}
	letters[y][x] = ch;
	return CIO_OK;
}

static void invert_cell(byte x, byte y)
{
	volatile byte * pixels = abm_row(&display->bitmap, y << 3) + (x >> 1);
	byte mask = x & 1 ? 15 : 240;
	for (byte i = 0; i < 8; i++)
	{
		*pixels ^= mask;
		pixels += display->bitmap.pitch;
	}
}

static bool matches(byte x, byte y, byte ch)
{
	const byte * glyph = cell_font(ch);
	byte inverse = ch & 128 ? 15 : 0;
	byte shift = x & 1 ? 0 : 4;
	volatile byte * pixels = abm_row(&display->bitmap, y << 3) + (x >> 1);
	for (byte i = 0; i < 8; i++)
	{
		if (((*pixels >> shift) & 15) != ((glyph[i] & 15) ^ inverse))
			return false;
		pixels += display->bitmap.pitch;
	}
	return true;
}

static void copy_cell(byte from, byte to, byte y)
{
	volatile byte * scan = abm_row(&display->bitmap, y << 3);
	byte from_shift = from & 1 ? 0 : 4;
	byte to_shift = to & 1 ? 0 : 4;
	byte keep = ~(15 << to_shift);
	for (byte i = 0; i < 8; i++)
	{
		byte ink = (scan[from >> 1] >> from_shift) & 15;
		volatile byte * target = scan + (to >> 1);
		*target = (*target & keep) | (ink << to_shift);
		scan += display->bitmap.pitch;
	}
	letters[y][to] = letters[y][from];
}

static byte shift_cells(byte start, byte y, bool insert)
{
	byte end = os_rmargn;
	for (byte i = start; i < end; i++)
	{
		byte x = insert ? end - (i - start) : i;
		copy_cell(insert ? x - 1 : x + 1, x, y);
	}
	return set_cell(insert ? start : end, y, ' ');
}

static byte clear_row(byte row)
{
	if (row >= 24)
		return CIO_CURSOR;
	abm_fill_rect(&display->bitmap, 0, row << 3, 320, 8, 0);
	for (byte x = 0; x < 80; x++)
		letters[row][x] = ' ';
	return CIO_OK;
}

static byte copy_row(byte from, byte to)
{
	if (from >= 24 || to >= 24)
		return CIO_CURSOR;
	ABitmapRect rect = {0, from << 3, 320, 8};
	if (!abm_copy_rect(&display->bitmap, 0, to << 3, &display->bitmap, &rect))
		return CIO_NOTIMPL;
	for (byte x = 0; x < 80; x++)
		letters[to][x] = letters[from][x];
	return CIO_OK;
}

static byte scroll(byte from, bool up)
{
	if (from >= 24)
		return CIO_CURSOR;
	for (byte i = from; i < 23; i++)
	{
		byte y = up ? i : 23 - (i - from);
		byte status = copy_row(up ? y + 1 : y - 1, y);
		if (status != CIO_OK)
			return status;
	}
	return clear_row(up ? 23 : from);
}

static void clear(void)
{
	abm_clear(&display->bitmap, 0);
	for (byte y = 0; y < 24; y++)
		for (byte x = 0; x < 80; x++)
			letters[y][x] = ' ';
}

static byte line_feed(void)
{
	os_colcrs = os_lmargn;
	if (os_rowcrs < 23)
		os_rowcrs++;
	else
		return scroll(0, true);
	return CIO_OK;
}

static bool text_position(void)
{
	return os_rowcrs < 24 && os_colcrs < 80 && os_lmargn <= os_rmargn && os_rmargn < 80;
}

static byte text_put(byte ch, bool wrap)
{
	if (!text_position())
		return CIO_CURSOR;
	byte x = os_colcrs, y = os_rowcrs;
	if (ch == 0x9b)
		return line_feed();
	if (!escaped && !os_dspflg)
	{
		switch (ch)
		{
		case 0x1b: escaped = true; return CIO_OK;
		case 0x1c: os_rowcrs = y ? y - 1 : 23; return CIO_OK;
		case 0x1d: os_rowcrs = y < 23 ? y + 1 : 0; return CIO_OK;
		case 0x1e: os_colcrs = x > os_lmargn ? x - 1 : os_rmargn; return CIO_OK;
		case 0x1f: os_colcrs = x < os_rmargn ? x + 1 : os_lmargn; return CIO_OK;
		case 0x7d: clear(); os_rowcrs = 0; os_colcrs = os_lmargn; return CIO_OK;
		case 0x7e: if (x > os_lmargn) os_colcrs = --x; return set_cell(x, y, ' ');
		case 0x7f: os_colcrs = x + 10 < os_rmargn ? x + 10 : os_rmargn; return CIO_OK;
		case 0x9e: os_colcrs = os_lmargn; return CIO_OK;
		case 0x9f: os_colcrs = x > os_lmargn + 10 ? x - 10 : os_lmargn; return CIO_OK;
		case 0x9c: return scroll(y, true);
		case 0x9d: return scroll(y, false);
		case 0xfe: return shift_cells(x, y, false);
		case 0xff: return shift_cells(x, y, true);
		}
	}
	escaped = false;
	byte status = set_cell(x, y, ch);
	if (status != CIO_OK)
		return status;
	if (x >= os_rmargn)
		return wrap ? line_feed() : CIO_OK;
	os_colcrs = x + 1;
	return CIO_OK;
}

__asm keyboard_get
{
	ldx entry_x
	lda $e425
	pha
	lda $e424
	pha
	rts
}

static byte read_key(void)
{
	byte aux = ZIOCB.ax1;
	ZIOCB.ax1 = 12;
	__asm volatile {
		jsr keyboard_get
		sta entry_a
		sty reply
	}
	ZIOCB.ax1 = aux;
	return reply;
}

static byte text_get(void)
{
	if (input_mode == 14)
		return read_key();
	if (!text_position())
		return CIO_CURSOR;
	byte y = os_rowcrs;
	if (ZIOCB.com == CIO_CMD_GETCHR && !input_pending)
	{
		byte x = os_colcrs;
		entry_a = letters[y][x];
		if (matches(x, y, entry_a))
			return CIO_OK;
		for (word i = 0; i < 256; i++)
		{
			if (matches(x, y, (byte)i))
			{
				entry_a = i;
				return CIO_OK;
			}
		}
		return 144;
	}
	if (!input_pending)
	{
		in_pos = os_colcrs;
		if (input_mode == 12)
		{
			for (;;)
			{
				byte x = os_colcrs;
				bool cursor = !os_crsinh;
				if (cursor)
					invert_cell(x, os_rowcrs);
				byte status = read_key();
				if (cursor)
					invert_cell(x, os_rowcrs);
				if (status >= 128 || entry_a == 0x9b)
				{
					if (status >= 128)
						return status;
					break;
				}
				text_put(entry_a, false);
			}
			if (y != os_rowcrs)
				in_pos = os_lmargn;
			y = os_rowcrs;
		}
		in_end = os_rmargn + 1;
		while (in_end > in_pos && letters[y][in_end - 1] == ' ')
			in_end--;
		input_pending = true;
	}
	if (in_pos < in_end)
	{
		entry_a = letters[y][in_pos++];
		return CIO_OK;
	}
	input_pending = false;
	entry_a = 0x9b;
	return line_feed();
}

// The ROM S: special entry implements GRAPHICS 8 DRAWTO and FILLTO.
// This uses the display address and cursor already set for device 8.
__asm screen_special
{
	ldx entry_x
	lda $e41b
	pha
	lda $e41a
	pha
	rts
}

static byte special(void)
{
	byte cmd = ZIOCB.com, x = ZIOCB.ax1, y = ZIOCB.ax2;
	switch (cmd)
	{
	case 17:
	case 18:
		if (!mode)
			return 145;
		if (os_colcrs >= 320 || os_rowcrs >= 192)
			return CIO_CURSOR;
		{
			word saved_col = OLDCOL;
			byte saved_row = OLDROW;
			OLDCOL = draw_x;
			OLDROW = draw_y;
			__asm volatile {
				jsr screen_special
				sty reply
			}
			OLDCOL = saved_col;
			OLDROW = saved_row;
		}
		draw_x = os_colcrs;
		draw_y = os_rowcrs;
		return reply;
	case 38:
		iocb[channel].ax3 = os_colcrs;
		iocb[channel].ax4 = os_colcrs >> 8;
		iocb[channel].ax5 = os_rowcrs;
		return CIO_OK;
	case 43:
		{
			word column = os_colcrs;
			byte row = os_rowcrs;
			os_colcrs = other_col;
			os_rowcrs = other_row;
			other_col = column;
			other_row = row;
			mode ^= 1;
			return CIO_OK;
		}
	case 45:
		input_mode = x == 13 || x == 14 ? x : 12;
		input_pending = false;
		return CIO_OK;
	}
	if (mode)
		return 145;
	switch (cmd)
	{
	case 39: return copy_row(x, y);
	case 40: return clear_row(x);
	case 41: return scroll(x, true);
	case 42: return scroll(x, false);
	case 44:
		if (x >= 80 || y >= 24)
			return CIO_CURSOR;
		invert_cell(x, y);
		letters[y][x] ^= 128;
		return CIO_OK;
	default: return CIO_NOTIMPL;
	}
}

static void restore(void)
{
	os_wait_frame();
	os_sdlst = old_list;
	os_sdmctl = old_dma;
	os_gprior = old_priority;
	os_color[1] = old_colors[0];
	os_color[2] = old_colors[1];
	os_color[4] = old_colors[2];
	os_lmargn = old_left;
	os_rmargn = old_right;
	os_dindex = old_mode;
	os_savmsc = old_screen;
	os_colcrs = old_col;
	os_rowcrs = old_row;
}

static byte open_channel(void)
{
	if (opened)
		return CIO_ISOPEN;
	channel = entry_x >> 4;
	old_list = os_sdlst;
	old_dma = os_sdmctl;
	old_priority = os_gprior;
	old_colors[0] = os_color[1];
	old_colors[1] = os_color[2];
	old_colors[2] = os_color[4];
	old_left = os_lmargn;
	old_right = os_rmargn;
	old_mode = os_dindex;
	old_screen = os_savmsc;
	old_col = os_colcrs;
	old_row = os_rowcrs;
	byte requested = ZIOCB.ax1;
	input_mode = requested == 13 || requested == 14 ? requested : 12;
	escaped = input_pending = false;
	other_col = os_colcrs = draw_x = 0;
	other_row = os_rowcrs = draw_y = mode = 0;
	os_lmargn = 0;
	os_rmargn = 79;
	os_dindex = 8;
	os_savmsc = (word)display->bitmap.data;
	clear();
	os_color[1] = 0x0e;
	os_color[2] = os_color[4] = 0;
	os_gprior &= 0x3f;
	abm_page_show(display);
	opened = true;
	return CIO_OK;
}

static __noinline byte process_entry(void)
{
	byte op = entry;
	if (!op)
		return open_channel();
	if (!opened || entry_x >> 4 != channel)
		return CIO_NOTOPEN;
	switch (op)
	{
	case 1:
		restore();
		opened = false;
		return CIO_OK;
	case 2:
	case 3:
		while (op == 3 && SSFLAG && BRKKEY)
			;
		if (!BRKKEY)
		{
			BRKKEY = 255;
			return CIO_BREAK;
		}
		if (!mode)
			return op == 2 ? text_get() : text_put(entry_a, true);
		if (os_colcrs >= 320 || os_rowcrs >= 192)
			return CIO_CURSOR;
		if (op == 2)
			entry_a = abm_get(&display->bitmap, os_colcrs, os_rowcrs);
		else
		{
			draw_x = os_colcrs;
			draw_y = os_rowcrs;
			*(volatile byte *)0x02fb = entry_a;
			abm_put(&display->bitmap, draw_x, draw_y, entry_a);
		}
		return CIO_OK;
	case 4:
		return mode;
	default:
		{
			byte status = special();
			iocb[channel].ax1 = ZIOCB.ax1 = input_mode;
			return status;
		}
	}
}

static __noinline void handle_entry(void)
{
	reply = process_entry();
}

__asm cio80_dispatch
{
	sta entry_a
	stx entry_x
	sty entry
	ldx #95
	save:
	lda $80,x
	sta saved_zp,x
	dex
	bpl save
	php
	cld
	jsr handle_entry
	plp
	ldx #95
	load:
	lda saved_zp,x
	sta $80,x
	dex
	bpl load
	lda entry_a
	ldx entry_x
	ldy reply
	rts
}

__asm cio80_open_entry
{
	ldy #0
	jmp cio80_dispatch
}
__asm cio80_close_entry
{
	ldy #1
	jmp cio80_dispatch
}
__asm cio80_get_entry
{
	ldy #2
	jmp cio80_dispatch
}
__asm cio80_put_entry
{
	ldy #3
	jmp cio80_dispatch
}
__asm cio80_status_entry
{
	ldy #4
	jmp cio80_dispatch
}
__asm cio80_special_entry
{
	ldy #5
	jmp cio80_dispatch
}

byte cio80_install(const ABitmapDisplayPage * page, const byte * font)
{
	if (installed)
		return CIO_ISOPEN;
	if (!page || !font || !page->list || !page->bitmap.data ||
		page->bitmap.width != 320 || page->bitmap.height != 192 ||
		page->bitmap.bits != 1 || page->bitmap.pitch != 40 || page->playfield != DMACTL_PF_NORMAL)
		return CIO_NOTIMPL;
	byte slot = 33;
	for (byte i = 0; i < 33; i += 3)
	{
		byte device = HANDLERS[i];
		if (device == '8')
			return CIO_ISOPEN;
		if (!device && slot == 33)
			slot = i;
	}
	if (slot == 33)
		return CIO_TMOF;
	const word vectors[] = {
		(word)cio80_open_entry - 1, (word)cio80_close_entry - 1, (word)cio80_get_entry - 1,
		(word)cio80_put_entry - 1, (word)cio80_status_entry - 1, (word)cio80_special_entry - 1
	};
	for (byte i = 0; i < 6; i++)
	{
		vector_table[2 * i] = vectors[i];
		vector_table[2 * i + 1] = vectors[i] >> 8;
	}
	vector_table[12] = 0x60;
	word address = (word)&vector_table[0];
	display = page;
	font_data = font;
	register_slot = slot;
	HANDLERS[slot + 1] = address;
	HANDLERS[slot + 2] = address >> 8;
	HANDLERS[slot] = '8';
	installed = true;
	return CIO_OK;
}

byte cio80_remove(void)
{
	if (!installed)
		return CIO_NODEV;
	if (opened)
		return CIO_ISOPEN;
	HANDLERS[register_slot] = 0;
	HANDLERS[register_slot + 1] = HANDLERS[register_slot + 2] = 0;
	installed = false;
	return CIO_OK;
}
