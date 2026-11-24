#include "charwin.h"
#include "input.h"

// XOR masks for bits 5 and 6, indexed by (ch >> 5) & 3.
static const byte a2s_x[4] = {0x40, 0x20, 0x60, 0x00};		// ATASCII to screen code
static const byte s2a_x[4] = {0x20, 0x60, 0x40, 0x00};		// Screen code to ATASCII

// Static helper arguments keep the fastcall parameter frames small.
// Call this module from the main program only.
static CharWin *		cw;					// window for the current operation
static byte				cw_x, cw_y, cw_w, cw_h;	// cell or rectangle
static byte				cw_c;				// screen code, editor key, cursor flag, or replacement code
static byte				cw_m;				// write, read, rectangle, or cursor mode
static byte				cw_by;				// scroll distance
static const byte *		cw_src;				// write source
static byte *			cw_dst;				// read or rectangle buffer
static word				cw_n;				// character count
static volatile byte *	cw_sp;				// parent window origin in memory
static byte				cw_p;				// parent window pitch
static byte				cw_pw, cw_ph;		// parent window dimensions
static byte				cw_psx, cw_psy;		// parent window position on screen

static __forceinline byte a2s(byte ch)
{
	return ch ^ a2s_x[(ch >> 5) & 3];
}

static __forceinline byte s2a(byte ch)
{
	return ch ^ s2a_x[(ch >> 5) & 3];
}

byte cwin_a2s(byte ch)
{
	return a2s(ch);
}

byte cwin_s2a(byte ch)
{
	return s2a(ch);
}

// Use a word for the row offset so large pitches do not wrap at 256 bytes.
static __forceinline volatile byte * cell(CharWin * win, byte x, byte y)
{
	return win->sp + (word)y * win->pitch + x;
}

// Showing and hiding are the same operation. Flip bit 7 or swap the
// cell with the saved byte. The caller tracks visibility.
static __forceinline void cur_flip(CharWin * win)
{
	volatile byte * p = cell(win, win->cx, win->cy);
	if (win->fl & CWIN_CODE)
	{
		byte t = *p;
		*p = win->cb;
		win->cb = t;
	}
	else
		*p ^= 0x80;
}

// Call before and after changing cells while the cursor is visible.
static void cur_tog(void)
{
	CharWin * win = cw;
	if (win->fl & CWIN_CURSOR)
		cur_flip(win);
}

// Recover the cell underneath a visible cursor.
static __forceinline byte under(CharWin * win, byte c)
{
	return (win->fl & CWIN_CODE) ? win->cb : c ^ 0x80;
}

// Clip to the parent and reset the new window's cursor state. The parent
// fields are already copied, so the destination can be the parent itself.
static void win_set(void)
{
	CharWin * win = cw;
	byte sx = cw_x, sy = cw_y, wx = cw_w, wy = cw_h, pw = cw_pw, ph = cw_ph, p = cw_p;
	if (sx >= pw || sy >= ph)
	{
		sx = 0; sy = 0; wx = 0; wy = 0;
	}
	if ((word)sx + wx > pw)
		wx = pw - sx;
	if ((word)sy + wy > ph)
		wy = ph - sy;
	if (!wx || !wy)
		wx = wy = 0;
	win->sx = cw_psx + sx; win->sy = cw_psy + sy; win->wx = wx; win->wy = wy;
	win->cx = 0; win->cy = 0; win->fl = 0; win->pitch = p;
	win->sp = cw_sp + (word)sy * p + sx;
}

void cwin_init(CharWin * win, volatile byte * screen, byte sx, byte sy, byte wx, byte wy)
{
	cw_sp = screen; cw_p = CWIN_COLS; cw_pw = CWIN_COLS; cw_ph = CWIN_ROWS; cw_psx = 0; cw_psy = 0;
	cw = win; cw_x = sx; cw_y = sy; cw_w = wx; cw_h = wy;
	win_set();
}

void cwin_init_screen(CharWin * win, volatile byte * screen, byte pitch, byte rows)
{
	cw_sp = screen; cw_p = pitch; cw_pw = pitch; cw_ph = rows; cw_psx = 0; cw_psy = 0;
	cw = win; cw_x = 0; cw_y = 0; cw_w = pitch; cw_h = rows;
	win_set();
}

void cwin_init_sub(CharWin * win, CharWin * parent, byte sx, byte sy, byte wx, byte wy)
{
	cw_sp = parent->sp; cw_p = parent->pitch; cw_pw = parent->wx; cw_ph = parent->wy; cw_psx = parent->sx; cw_psy = parent->sy;
	cw = win; cw_x = sx; cw_y = sy; cw_w = wx; cw_h = wy;
	win_set();
}

// Fill the clipped rectangle with cw_c. The caller handles the cursor.
static void fill_raw(void)
{
	CharWin * win = cw;
	byte x = cw_x, y = cw_y, w = cw_w, h = cw_h, s = cw_c;
	if (x >= win->wx || y >= win->wy)
		return;
	if ((word)x + w > win->wx) w = win->wx - x;
	if ((word)y + h > win->wy) h = win->wy - y;
	for (byte j = 0; j < h; j++)
	{
		volatile byte * p = cell(win, x, y + j);
		for (byte i = 0; i < w; i++)
			p[i] = s;
	}
}

// Fill the whole window with cw_c.
static void fill_win(void)
{
	cw_x = 0; cw_y = 0; cw_w = cw->wx; cw_h = cw->wy;
	fill_raw();
}

void cwin_fill(CharWin * win, byte ch)
{
	cw = win;
	cur_tog();
	cw_c = a2s(ch);
	fill_win();
	cur_tog();
}

static void clear_c(void)
{
	CharWin * win = cw;
	cur_tog();
	cw_c = 0;
	fill_win();
	win->cx = 0; win->cy = 0;
	win->fl &= ~CWIN_FULL;
	cur_tog();
}

void cwin_clear(CharWin * win)
{
	cw = win;
	clear_c();
}

// Apply the visibility flag in cw_c.
static void cur_show(void)
{
	CharWin * win = cw;
	if (!win->wx)
		return;
	if (cw_c != !!(win->fl & CWIN_CURSOR))
	{
		cur_flip(win);
		win->fl ^= CWIN_CURSOR;
	}
}

void cwin_cursor_show(CharWin * win, bool show)
{
	cw = win;
	cw_c = show;
	cur_show();
}

// Remove the cursor in its old style, then apply the new style in cw_m.
static void cur_style(void)
{
	CharWin * win = cw;
	cur_tog();
	win->fl = (win->fl & ~CWIN_CODE) | cw_m;
	if (cw_m)
		win->cb = cw_c;
	cur_tog();
}

void cwin_cursor_code(CharWin * win, byte cc)
{
	cw = win; cw_c = cc; cw_m = CWIN_CODE; cur_style();
}

void cwin_cursor_xor(CharWin * win)
{
	cw = win; cw_m = 0; cur_style();
}

void cwin_cursor_move(CharWin * win, byte cx, byte cy)
{
	if (!win->wx)
		return;
	if (cx >= win->wx) cx = win->wx - 1;
	if (cy >= win->wy) cy = win->wy - 1;
	cw = win;
	cur_tog();
	win->cx = cx; win->cy = cy;
	win->fl &= ~CWIN_FULL;
	cur_tog();
}

// Movement helpers report whether the cursor moved. The caller draws it.
static bool mv_fwd(void)
{
	CharWin * win = cw;
	if (win->cx + 1 < win->wx)
		win->cx++;
	else if (win->cy + 1 < win->wy)
	{
		win->cx = 0;
		win->cy++;
	}
	else
		return false;
	return true;
}

static bool mv_back(void)
{
	CharWin * win = cw;
	if (win->cx)
		win->cx--;
	else if (win->cy)
	{
		win->cx = win->wx - 1;
		win->cy--;
	}
	else
		return false;
	return true;
}

static bool mv_l(void)
{
	CharWin * win = cw;
	if (!win->cx)
		return false;
	win->cx--;
	return true;
}

static bool mv_r(void)
{
	CharWin * win = cw;
	if (win->cx + 1 >= win->wx)
		return false;
	win->cx++;
	return true;
}

static bool mv_u(void)
{
	CharWin * win = cw;
	if (!win->cy)
		return false;
	win->cy--;
	return true;
}

static bool mv_d(void)
{
	CharWin * win = cw;
	if (win->cy + 1 >= win->wy)
		return false;
	win->cy++;
	return true;
}

static bool mv_nl(void)
{
	CharWin * win = cw;
	if (win->cy + 1 >= win->wy)
		return false;
	win->cx = 0;
	win->cy++;
	return true;
}

// A successful move clears CWIN_FULL. Restore the cursor after the move.
static __forceinline bool moved(bool ok)
{
	if (ok)
		cw->fl &= ~CWIN_FULL;
	cur_tog();
	return ok;
}

// Arrow movements are shared by the editor.
static bool c_left(void)		{ cur_tog(); return moved(mv_l()); }
static bool c_right(void)		{ cur_tog(); return moved(mv_r()); }
static bool c_up(void)			{ cur_tog(); return moved(mv_u()); }
static bool c_down(void)		{ cur_tog(); return moved(mv_d()); }

// An empty window blocks all movement and has no visible cursor.
bool cwin_cursor_left(CharWin * win)		{ cw = win; return c_left(); }
bool cwin_cursor_right(CharWin * win)		{ cw = win; return c_right(); }
bool cwin_cursor_up(CharWin * win)			{ cw = win; return c_up(); }
bool cwin_cursor_down(CharWin * win)		{ cw = win; return c_down(); }
bool cwin_cursor_forward(CharWin * win)		{ cw = win; cur_tog(); return moved(win->wx && mv_fwd()); }
bool cwin_cursor_backward(CharWin * win)	{ cw = win; cur_tog(); return moved(mv_back()); }
bool cwin_cursor_newline(CharWin * win)		{ cw = win; cur_tog(); return moved(mv_nl()); }

// Write mode flags select ATASCII conversion and stopping at byte 0.
#define PM_CONV		1
#define PM_STOP0	2

// Write from the cursor. The caller handles the visible cursor.
static word put_n(void)
{
	CharWin * win = cw;
	const byte * src = cw_src;
	word n = cw_n;
	bool conv = cw_m & PM_CONV, stop0 = cw_m & PM_STOP0;
	word k = 0;
	if (!win->wx)
		return 0;
	while (k < n && !(win->fl & CWIN_FULL))
	{
		byte c = src[k];
		if (stop0 && !c)
			break;
		*cell(win, win->cx, win->cy) = conv ? a2s(c) : c;
		k++;
		if (!mv_fwd())
			win->fl |= CWIN_FULL;
	}
	return k;
}

// Write one screen code at the cursor for the editor and console.
static void put1(void)
{
	CharWin * win = cw;
	if (!win->wx || (win->fl & CWIN_FULL))
		return;
	*cell(win, win->cx, win->cy) = cw_c;
	if (!mv_fwd())
		win->fl |= CWIN_FULL;
}

static word put_c(void)
{
	cur_tog();
	word k = put_n();
	cur_tog();
	return k;
}

void cwin_put_char(CharWin * win, byte ch)							{ cw = win; cw_src = &ch; cw_n = 1; cw_m = PM_CONV; put_c(); }
word cwin_put_chars(CharWin * win, const byte * chars, word num)	{ cw = win; cw_src = chars; cw_n = num; cw_m = PM_CONV; return put_c(); }
word cwin_put_string(CharWin * win, const char * str)				{ cw = win; cw_src = (const byte *)str; cw_n = 0xffff; cw_m = PM_CONV | PM_STOP0; return put_c(); }
void cwin_put_char_raw(CharWin * win, byte ch)						{ cw = win; cw_src = &ch; cw_n = 1; cw_m = 0; put_c(); }
word cwin_put_chars_raw(CharWin * win, const byte * chars, word num){ cw = win; cw_src = chars; cw_n = num; cw_m = 0; return put_c(); }
word cwin_put_string_raw(CharWin * win, const char * str)			{ cw = win; cw_src = (const byte *)str; cw_n = 0xffff; cw_m = PM_STOP0; return put_c(); }

// Write within one row, stopping at the window edge.
static word putat_n(void)
{
	CharWin * win = cw;
	byte x = cw_x, y = cw_y, mode = cw_m;
	const byte * src = cw_src;
	word n = cw_n;
	if (x >= win->wx || y >= win->wy)
		return 0;
	word m = win->wx - x;
	if (n > m)
		n = m;
	cur_tog();
	volatile byte * p = cell(win, x, y);
	word k = 0;
	while (k < n)
	{
		byte c = src[k];
		if ((mode & PM_STOP0) && !c)
			break;
		p[k] = (mode & PM_CONV) ? a2s(c) : c;
		k++;
	}
	cur_tog();
	return k;
}

void cwin_putat_char(CharWin * win, byte x, byte y, byte ch)
{
	cw = win; cw_x = x; cw_y = y; cw_src = &ch; cw_n = 1; cw_m = PM_CONV; putat_n();
}
word cwin_putat_chars(CharWin * win, byte x, byte y, const byte * chars, word num)
{
	cw = win; cw_x = x; cw_y = y; cw_src = chars; cw_n = num; cw_m = PM_CONV; return putat_n();
}
word cwin_putat_string(CharWin * win, byte x, byte y, const char * str)
{
	cw = win; cw_x = x; cw_y = y; cw_src = (const byte *)str; cw_n = 0xffff; cw_m = PM_CONV | PM_STOP0; return putat_n();
}
void cwin_putat_char_raw(CharWin * win, byte x, byte y, byte ch)
{
	cw = win; cw_x = x; cw_y = y; cw_src = &ch; cw_n = 1; cw_m = 0; putat_n();
}
word cwin_putat_chars_raw(CharWin * win, byte x, byte y, const byte * chars, word num)
{
	cw = win; cw_x = x; cw_y = y; cw_src = chars; cw_n = num; cw_m = 0; return putat_n();
}
word cwin_putat_string_raw(CharWin * win, byte x, byte y, const char * str)
{
	cw = win; cw_x = x; cw_y = y; cw_src = (const byte *)str; cw_n = 0xffff; cw_m = PM_STOP0; return putat_n();
}

// Read the cell under any cursor drawn by this window.
static __forceinline byte rd(CharWin * win, byte x, byte y)
{
	byte c = *cell(win, x, y);
	if ((win->fl & CWIN_CURSOR) && x == win->cx && y == win->cy)
		c = under(win, c);
	return c;
}

// Read within one row, stopping at the window edge.
static word getat_n(void)
{
	CharWin * win = cw;
	byte x = cw_x, y = cw_y;
	byte * dst = cw_dst;
	word n = cw_n;
	bool conv = cw_m;
	if (x >= win->wx || y >= win->wy)
		return 0;
	word m = win->wx - x;
	if (n > m)
		n = m;
	// Check the cursor against the row coordinates while reading.
	volatile byte * p = cell(win, x, y);
	for (word k = 0; k < n; k++)
	{
		byte c = p[k];
		if ((win->fl & CWIN_CURSOR) && x + (byte)k == win->cx && y == win->cy)
			c = under(win, c);
		dst[k] = conv ? s2a(c) : c;
	}
	return n;
}

byte cwin_getat_char(CharWin * win, byte x, byte y)
{
	byte c = 0;
	cw = win; cw_x = x; cw_y = y; cw_dst = &c; cw_n = 1; cw_m = true;
	getat_n();
	return c;
}

byte cwin_getat_char_raw(CharWin * win, byte x, byte y)
{
	byte c = 0;
	cw = win; cw_x = x; cw_y = y; cw_dst = &c; cw_n = 1; cw_m = false;
	getat_n();
	return c;
}

word cwin_getat_chars(CharWin * win, byte x, byte y, byte * chars, word num)
{
	cw = win; cw_x = x; cw_y = y; cw_dst = chars; cw_n = num; cw_m = true; return getat_n();
}
word cwin_getat_chars_raw(CharWin * win, byte x, byte y, byte * chars, word num)
{
	cw = win; cw_x = x; cw_y = y; cw_dst = chars; cw_n = num; cw_m = false; return getat_n();
}

void cwin_read_string(CharWin * win, char * buffer)
{
	word k = 0;
	for (byte y = 0; y < win->wy; y++)
		for (byte x = 0; x < win->wx; x++)
			buffer[k++] = s2a(rd(win, x, y));
	buffer[k] = 0;
}

// Clip the screen area and keep w as the buffer row stride.
// Mode values 0 to 3 select put, put raw, get, and get raw.
static void rect(void)
{
	CharWin * win = cw;
	byte x = cw_x, y = cw_y, w = cw_w, h = cw_h, mode = cw_m;
	byte * buf = cw_dst;
	if (x >= win->wx || y >= win->wy)
		return;
	byte nw = w, nh = h;		// clipped dimensions
	if ((word)x + nw > win->wx) nw = win->wx - x;
	if ((word)y + nh > win->wy) nh = win->wy - y;
	cur_tog();
	for (byte j = 0; j < nh; j++)
	{
		volatile byte * p = cell(win, x, y + j);
		byte * b = buf + (word)j * w;
		for (byte i = 0; i < nw; i++)
		{
			switch (mode)
			{
			case 0: p[i] = a2s(b[i]); break;
			case 1: p[i] = b[i]; break;
			case 2: b[i] = s2a(p[i]); break;
			default: b[i] = p[i]; break;
			}
		}
	}
	cur_tog();
}

void cwin_put_rect(CharWin * win, byte x, byte y, byte w, byte h, const byte * chars)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_dst = (byte *)chars; cw_m = 0; rect();
}
void cwin_put_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, const byte * chars)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_dst = (byte *)chars; cw_m = 1; rect();
}
void cwin_get_rect(CharWin * win, byte x, byte y, byte w, byte h, byte * chars)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_dst = chars; cw_m = 2; rect();
}
void cwin_get_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, byte * chars)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_dst = chars; cw_m = 3; rect();
}

// Fill while preserving the cursor.
static void fill_rect(void)
{
	cur_tog();
	fill_raw();
	cur_tog();
}

void cwin_fill_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, byte ch)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_c = ch; fill_rect();
}

void cwin_fill_rect(CharWin * win, byte x, byte y, byte w, byte h, byte ch)
{
	cw = win; cw_x = x; cw_y = y; cw_w = w; cw_h = h; cw_c = a2s(ch); fill_rect();
}

static void ins(void)
{
	CharWin * win = cw;
	byte s = cw_c;
	volatile byte * p = cell(win, 0, win->cy);
	for (byte i = win->wx - 1; i > win->cx; i--)
		p[i] = p[i - 1];
	p[win->cx] = s;
}

static void del(void)
{
	CharWin * win = cw;
	volatile byte * p = cell(win, 0, win->cy);
	byte last = win->wx - 1;
	for (byte i = win->cx; i < last; i++)
		p[i] = p[i + 1];
	p[last] = 0;
}

// Insert or delete a cell while preserving the cursor.
static void ins_c(void)
{
	if (!cw->wx)
		return;
	cur_tog();
	ins();
	cur_tog();
}

static void del_c(void)
{
	if (!cw->wx)
		return;
	cur_tog();
	del();
	cur_tog();
}

void cwin_insert_char_raw(CharWin * win, byte ch)
{
	cw = win;
	cw_c = ch;
	ins_c();
}

void cwin_insert_char(CharWin * win, byte ch)
{
	cw = win;
	cw_c = a2s(ch);
	ins_c();
}

void cwin_delete_char(CharWin * win)
{
	cw = win;
	del_c();
}

// Scroll up by cw_by. The caller handles the cursor.
static void sc_up(void)
{
	CharWin * win = cw;
	byte by = cw_by;
	for (byte y = 0; y + by < win->wy; y++)
	{
		volatile byte * d = cell(win, 0, y);
		volatile byte * s = cell(win, 0, y + by);
		for (byte i = 0; i < win->wx; i++)
			d[i] = s[i];
	}
}

void cwin_scroll_up(CharWin * win, byte by)
{
	if (!by || by >= win->wy)
		return;
	cw = win;
	cur_tog();
	cw_by = by;
	sc_up();
	cur_tog();
}

void cwin_scroll_down(CharWin * win, byte by)
{
	if (!by || by >= win->wy)
		return;
	cw = win;
	cur_tog();
	for (byte y = win->wy - 1; y >= by; y--)
	{
		volatile byte * d = cell(win, 0, y);
		volatile byte * s = cell(win, 0, y - by);
		for (byte i = 0; i < win->wx; i++)
			d[i] = s[i];
	}
	cur_tog();
}

void cwin_scroll_left(CharWin * win, byte by)
{
	if (!by || by >= win->wx)
		return;
	cw = win;
	cur_tog();
	for (byte y = 0; y < win->wy; y++)
	{
		volatile byte * p = cell(win, 0, y);
		for (byte i = 0; i + by < win->wx; i++)
			p[i] = p[i + by];
	}
	cur_tog();
}

void cwin_scroll_right(CharWin * win, byte by)
{
	if (!by || by >= win->wx)
		return;
	cw = win;
	cur_tog();
	for (byte y = 0; y < win->wy; y++)
	{
		volatile byte * p = cell(win, 0, y);
		for (byte i = win->wx - 1; i >= by; i--)
			p[i] = p[i - by];
	}
	cur_tog();
}

// ATASCII editor controls: $1B..$1F, $7D..$7F, $9B..$9F, $FD..$FF.
static __forceinline bool is_ctl(byte ch)
{
	byte c = ch & 0x7f;
	return (c >= 0x1b && c <= 0x1f) || c >= 0x7d;
}

// Handle one editor key.
static byte edit_c(void)
{
	CharWin * win = cw;
	byte ch = cw_c;
	if (!win->wx)
		return 0;
	switch (ch)
	{
	case ATASCII_LEFT:	c_left(); return 1;
	case ATASCII_RIGHT:	c_right(); return 1;
	case ATASCII_UP:	c_up(); return 1;
	case ATASCII_DOWN:	c_down(); return 1;
	case ATASCII_BS:
		cur_tog();
		if (mv_back())
		{
			*cell(win, win->cx, win->cy) = 0;
			win->fl &= ~CWIN_FULL;
		}
		cur_tog();
		return 1;
	case ATASCII_INSERT:
		cw_c = 0;
		ins_c();
		return 1;
	case ATASCII_DELETE:
		del_c();
		return 1;
	}
	if (is_ctl(ch))
		return 0;
	cur_tog();
	cw_c = a2s(ch);
	put1();
	cur_tog();
	return 1;
}

byte cwin_edit_char(CharWin * win, byte ch)
{
	cw = win;
	cw_c = ch;
	return edit_c();
}

byte cwin_edit(CharWin * win)
{
	if (!win->wx)
		return ATASCII_ESC;
	cw = win;
	cw_c = true;
	cur_show();
	byte ch;
	for (;;)
	{
		ch = key_getc();
		if (ch == ATASCII_EOL || ch == ATASCII_ESC)
			break;
		cw = win;
		cw_c = ch;
		edit_c();
	}
	cw = win;
	cw_c = false;
	cur_show();
	return ch;
}

// Scroll up one row and start a new line at the bottom.
static void con_scroll(void)
{
	CharWin * win = cw;
	cw_by = 1;
	sc_up();
	volatile byte * p = cell(win, 0, win->wy - 1);
	for (byte i = 0; i < win->wx; i++)
		p[i] = 0;
	win->cx = 0;
	win->cy = win->wy - 1;
}

static void con_nl(void)
{
	CharWin * win = cw;
	if (win->cy + 1 < win->wy)
	{
		win->cx = 0;
		win->cy++;
	}
	else
		con_scroll();
	win->fl &= ~CWIN_FULL;
}

// Write one ATASCII character to the window console.
static void con_char(void)
{
	CharWin * win = cw;
	byte ch = cw_c;
	if (ch == 10 || ch == ATASCII_EOL)
		con_nl();
	else
	{
		if (win->fl & CWIN_FULL)
		{
			con_scroll();
			win->fl &= ~CWIN_FULL;
		}
		cw_c = a2s(ch);
		put1();
	}
}

void cwin_console_write_char(CharWin * win, byte ch)
{
	if (!win->wx)
		return;
	cw = win;
	cur_tog();
	cw_c = ch;
	con_char();
	cur_tog();
}

void cwin_console_write_string(CharWin * win, const char * str)
{
	if (!win->wx)
		return;
	cw = win;
	cur_tog();
	while (*str)
	{
		cw_c = (byte)*str++;
		con_char();
	}
	cur_tog();
}

void cwin_console_newline(CharWin * win)
{
	if (!win->wx)
		return;
	cw = win;
	cur_tog();
	con_nl();
	cur_tog();
}

void cwin_console_clear(CharWin * win)
{
	cw = win;
	clear_c();
}
