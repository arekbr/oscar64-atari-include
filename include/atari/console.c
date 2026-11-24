#include "console.h"
#include "cio.h"
#include "fmt.h"
#include "os.h"

int con_gets(char * buf, byte size)
{
	if (size < 2)
		return -1;
	word done;
	byte st = cio_getrec(0, buf, size, &done);
	byte n = done;
	// Treat a truncated record as a successful read of its prefix.
	if (st >= 0x80 && st != CIO_TRUNC)
		return -1;
	// Drop the ATASCII EOL or reserve the last byte of a full buffer for '\0'.
	if (n && (buf[n - 1] == (char)0x9b || n == size))
		n--;
	buf[n] = 0;
	return n;
}

int con_getchar(void)
{
	// A zero-length GETCHR returns one byte in A. cio_read skips zero-length reads.
	iocb[0].com = CIO_CMD_GETCHR;
	iocb[0].bll = 0;
	word r = cio_raw(0);
	if ((r >> 8) >= 0x80)
		return -1;
	byte c = r;
	return c == 0x9b ? '\n' : c;
}

int con_printf(const char * fmt, ...)
{
	va_list	ap;
	va_start(ap, fmt);
	return fmt_vcprintf(0, fmt, ap);
}

// Static storage avoids a compiler assertion when inlining put1().
// Write through a volatile pointer because CIO reads the buffer.
#define CON_BUF		((volatile byte *)con_ch)

static byte con_ch[2];
static word con_done;
static byte con_dsp;

// Clear DSPFLG so E: interprets the control codes as commands.
static void ctl(const byte * s, byte n)
{
	con_dsp = os_dspflg;
	os_dspflg = 0;
	cio_write(0, s, n, &con_done);
	os_dspflg = con_dsp;
}

static void put1(byte c)
{
	CON_BUF[0] = c;
	ctl(con_ch, 1);
}

// A PUT restores the character at the old cursor position and redraws
// the cursor at ROWCRS/COLCRS. Cursor up followed by cursor down returns
// to the same cell, including at the screen edges and left of the margin.
static const byte con_redraw_seq[2] = {0x1c, 0x1d};

static void redraw(void)
{
	ctl(con_redraw_seq, 2);
}

void con_clrscr(void)
{
	put1(CH_CLR);
}

void con_gotoxy(byte x, byte y)
{
	if (x > 39)
		x = 39;
	if (y > 23)
		y = 23;
	os_rowcrs = y;
	os_colcrs = x;
	redraw();
}

byte con_wherex(void)
{
	return (byte)os_colcrs;
}

byte con_wherey(void)
{
	return os_rowcrs;
}

void con_clreol(void)
{
	byte x = (byte)os_colcrs;
	byte y = os_rowcrs;
	// Check the coordinates before writing directly to screen memory.
	if (x > 39 || y > 23)
		return;
	// Move the cursor off the row before clearing it so the next PUT
	// does not restore the old character over the cleared cell.
	os_rowcrs = y ? y - 1 : 1;
	redraw();
	volatile byte * p = (volatile byte *)os_savmsc + (word)y * 40 + x;
	for (byte n = 40 - x; n; n--)
		*p++ = 0;
	os_rowcrs = y;
	redraw();
}

void con_delline(void)
{
	put1(CH_DELLINE);
}

void con_insline(void)
{
	put1(CH_INSLINE);
}

void con_bell(void)
{
	put1(CH_BELL);
}

void con_cursor(byte on)
{
	os_crsinh = on ? 0 : 1;
	redraw();
}

void con_colors(byte text, byte bg)
{
	os_color[1] = text;
	os_color[2] = bg;
}

void con_border(byte color)
{
	os_color[4] = color;
}
