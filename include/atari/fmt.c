#include "fmt.h"
#include "cio.h"

// The formatter keeps its state in static storage.
// Internal helpers use that state to keep compiler call frames small.

#define FM_STR		0
#define FM_CIO		1

#define FM_RUN		0
#define FM_EIO		1		// return the negative CIO status
#define FM_EOVF		2		// formatted result exceeds the supported range

#define F_MINUS		0x01
#define F_PLUS		0x02
#define F_SPACE		0x04
#define F_ZERO		0x08
#define F_ALT		0x10
#define F_PREC		0x20

static byte			fm_mode;
static char *		fm_buf;			// destination buffer
static word			fm_lim;			// maximum payload in the destination
static word			fm_pos;			// bytes stored in the destination
static word			fm_cnt;			// total length, capped at 32767
static byte			fm_ovf;			// total length exceeds 32767
static byte			fm_stop;
static byte			fm_st;			// CIO status on FM_EIO
static byte			fm_ch;			// destination channel
static byte			fm_bn;			// bytes buffered for CIO
static byte			fm_blk[32];		// buffer read by the OS
#define FM_BLK		((volatile byte *)fm_blk)	// keep stores to the OS buffer visible

static const char *	fm_fmt;
static va_list		fm_ap;

static char			fm_c;			// current byte
static word			fm_n;			// byte count for fm_rep() and fm_lit()
static const char *	fm_s;			// source for fm_lit()

static byte			fm_fl;			// format specifier flags
static word			fm_w;			// width
static word			fm_p;			// precision (when F_PREC)
static word			fm_num;			// parsed number or width of the current output
static byte			fm_len;			// 0 = int, 1 = hh, 4 = l
static dword		fm_v;			// fm_int: absolute value
static byte			fm_neg;
static byte			fm_base;		// 8, 10, 16
static byte			fm_up;			// uppercase hex digits
static byte			fm_sgn;			// signed conversion (d and i)
static byte			fm_hex0x;		// "0x" prefix for %p
static byte			fm_dig[12];		// digits in reverse order

// Send the buffered block and stop on any CIO status of 128 or greater.
static void fm_flush(void)
{
	byte	st;

	if (fm_bn == 0)
		return;
	if (fm_ch > 7)
		st = CIO_BADIOCB;
	else
	{
		iocb[fm_ch].bal = (word)fm_blk;
		iocb[fm_ch].bll = fm_bn;
		iocb[fm_ch].com = CIO_CMD_PUTCHR;
		st = cio_raw(fm_ch) >> 8;
	}
	fm_bn = 0;
	if (st >= 128)
	{
		fm_st = st;
		fm_stop = FM_EIO;
	}
}

static void fm_put(void)
{
	if (fm_stop != FM_RUN)
		return;
	if (fm_cnt < 32767)
		fm_cnt++;
	else
		fm_ovf = 1;

	if (fm_mode == FM_STR)
	{
		if (fm_pos < fm_lim)
		{
			fm_buf[fm_pos] = fm_c;
			fm_pos++;
		}
		else if (fm_ovf)
			fm_stop = FM_EOVF;		// buffer full and the final result is already -1
	}
	else
	{
		FM_BLK[fm_bn] = fm_c == '\n' ? 0x9b : fm_c;
		fm_bn++;
		if (fm_bn == sizeof(fm_blk))
			fm_flush();
	}
}

// Emit fm_n copies of fm_c.
static void fm_rep(void)
{
	while (fm_n && fm_stop == FM_RUN)
	{
		fm_put();
		fm_n--;
	}
}

// Emit fm_n bytes starting at fm_s.
static void fm_lit(void)
{
	while (fm_n && fm_stop == FM_RUN)
	{
		fm_c = *fm_s++;
		fm_put();
		fm_n--;
	}
}

// Read a decimal field size, rejecting values above 32767.
static void fm_getnum(void)
{
	fm_num = 0;
	for (;;)
	{
		byte d = (byte)*fm_fmt - '0';
		if (d > 9)
			return;
		fm_fmt++;
		if (fm_num > 3276 || (fm_num == 3276 && d > 7))
		{
			fm_stop = FM_EOVF;
			return;
		}
		fm_num = fm_num * 10 + d;
	}
}

// Pad a field of fm_num bytes on the left or right.
static void fm_padl(void)
{
	if (!(fm_fl & F_MINUS) && fm_w > fm_num)
	{
		fm_c = ' ';
		fm_n = fm_w - fm_num;
		fm_rep();
	}
}

static void fm_padr(void)
{
	if ((fm_fl & F_MINUS) && fm_w > fm_num)
	{
		fm_c = ' ';
		fm_n = fm_w - fm_num;
		fm_rep();
	}
}

// Format the current integer and apply the requested padding.
static void fm_int(void)
{
	byte	nd = 0;
	dword	v = fm_v;

	if (fm_base == 10)
	{
		while (v > 0xffff)
		{
			fm_dig[nd++] = (byte)(v % 10);
			v /= 10;
		}
		word w = (word)v;
		while (w)
		{
			fm_dig[nd++] = (byte)(w % 10);
			w /= 10;
		}
	}
	else
	{
		byte	mask = fm_base - 1;
		byte	sh = fm_base == 8 ? 3 : 4;
		while (v)
		{
			fm_dig[nd++] = (byte)v & mask;
			v >>= sh;
		}
	}

	// A precision of zero suppresses the digits for a value of zero.
	word	prec = (fm_fl & F_PREC) ? fm_p : 1;
	word	zeros = prec > nd ? prec - nd : 0;
	if ((fm_fl & F_ALT) && fm_base == 8 && zeros == 0)
		zeros = 1;					// keep the leading zero for %#o

	char	c1 = 0, c2 = 0;
	if (fm_sgn)
	{
		if (fm_neg)
			c1 = '-';
		else if (fm_fl & F_PLUS)
			c1 = '+';
		else if (fm_fl & F_SPACE)
			c1 = ' ';
	}
	else if (fm_base == 16 && (fm_hex0x || ((fm_fl & F_ALT) && fm_v)))
	{
		c1 = '0';
		c2 = fm_up ? 'X' : 'x';
	}

	word	total = zeros + nd;
	if (c1)
		total++;
	if (c2)
		total++;
	// The zero flag applies when there is no precision or left alignment.
	if ((fm_fl & (F_ZERO | F_MINUS | F_PREC)) == F_ZERO && fm_w > total)
	{
		zeros += fm_w - total;
		total = fm_w;
	}

	fm_num = total;
	fm_padl();
	if (c1)
	{
		fm_c = c1;
		fm_put();
	}
	if (c2)
	{
		fm_c = c2;
		fm_put();
	}
	fm_c = '0';
	fm_n = zeros;
	fm_rep();
	while (nd && fm_stop == FM_RUN)
	{
		byte d = fm_dig[--nd];
		fm_c = d < 10 ? '0' + d : (fm_up ? 'A' - 10 : 'a' - 10) + d;
		fm_put();
	}
	fm_padr();
}

// Read a conversion starting just after the percent sign.
static void fm_spec(void)
{
	const char *	beg = fm_fmt - 1;
	char			c;

	// flags
	fm_fl = 0;
	for (;;)
	{
		c = *fm_fmt;
		if (c == '-')
			fm_fl |= F_MINUS;
		else if (c == '+')
			fm_fl |= F_PLUS;
		else if (c == ' ')
			fm_fl |= F_SPACE;
		else if (c == '0')
			fm_fl |= F_ZERO;
		else if (c == '#')
			fm_fl |= F_ALT;
		else
			break;
		fm_fmt++;
	}

	// width
	fm_w = 0;
	if (*fm_fmt == '*')
	{
		fm_fmt++;
		int a = va_arg(fm_ap, int);
		if (a < 0)
		{
			if (a == -32768)
			{
				fm_stop = FM_EOVF;
				return;
			}
			fm_fl |= F_MINUS;
			a = -a;
		}
		fm_w = a;
	}
	else
	{
		fm_getnum();
		if (fm_stop)
			return;
		fm_w = fm_num;
	}

	// precision
	if (*fm_fmt == '.')
	{
		fm_fmt++;
		if (*fm_fmt == '*')
		{
			fm_fmt++;
			int a = va_arg(fm_ap, int);
			if (a >= 0)
			{
				fm_fl |= F_PREC;
				fm_p = a;
			}
		}
		else
		{
			fm_getnum();
			if (fm_stop)
				return;
			fm_fl |= F_PREC;
			fm_p = fm_num;
		}
	}

	// length
	fm_len = 0;
	c = *fm_fmt;
	if (c == 'l')
	{
		fm_len = 4;
		fm_fmt++;
	}
	else if (c == 'h')
	{
		fm_fmt++;
		if (*fm_fmt == 'h')
		{
			fm_len = 1;
			fm_fmt++;
		}
	}

	c = *fm_fmt;
	if (c == 0)
	{
		// Copy the trailing incomplete conversion literally.
		fm_s = beg;
		fm_n = fm_fmt - beg;
		fm_lit();
		return;
	}
	fm_fmt++;

	fm_sgn = 0;
	fm_hex0x = 0;
	fm_up = 0;
	switch (c)
	{
	case 'd':
	case 'i':
		fm_sgn = 1;
		fm_base = 10;
		fm_fl &= ~F_ALT;
		{
			long	sv;
			if (fm_len == 4)
				sv = va_arg(fm_ap, long);
			else if (fm_len == 1)
				sv = (sbyte)va_arg(fm_ap, int);
			else
				sv = va_arg(fm_ap, int);
			fm_neg = sv < 0;
			fm_v = fm_neg ? -(dword)sv : (dword)sv;
		}
		fm_int();
		break;

	case 'u':
	case 'o':
	case 'x':
	case 'X':
		fm_fl &= ~(F_PLUS | F_SPACE);
		if (c == 'u')
		{
			fm_base = 10;
			fm_fl &= ~F_ALT;
		}
		else if (c == 'o')
			fm_base = 8;
		else
		{
			fm_base = 16;
			fm_up = c == 'X';
		}
		if (fm_len == 4)
			fm_v = va_arg(fm_ap, unsigned long);
		else if (fm_len == 1)
			fm_v = (byte)va_arg(fm_ap, int);
		else
			fm_v = (word)va_arg(fm_ap, int);
		fm_neg = 0;
		fm_int();
		break;

	case 'p':
		fm_fl &= F_MINUS;
		fm_fl |= F_PREC;
		fm_p = 4;
		fm_base = 16;
		fm_hex0x = 1;
		fm_neg = 0;
		fm_v = (word)va_arg(fm_ap, void *);
		fm_int();
		break;

	case 'c':
		fm_num = 1;
		fm_padl();
		fm_c = (char)va_arg(fm_ap, int);
		fm_put();
		fm_padr();
		break;

	case 's':
		{
			const char * s = va_arg(fm_ap, const char *);
			word n = 0;
			if (fm_fl & F_PREC)
			{
				while (n < fm_p && s[n])
					n++;
			}
			else
			{
				while (s[n])
					n++;
			}
			fm_num = n;
			fm_padl();
			fm_s = s;
			fm_n = n;
			fm_lit();
			fm_num = n;
			fm_padr();
		}
		break;

	case '%':
		fm_c = '%';
		fm_put();
		break;

	default:
		// Copy the unrecognized conversion literally.
		fm_s = beg;
		fm_n = fm_fmt - beg;
		fm_lit();
		break;
	}
}

static void fm_run(void)
{
	fm_cnt = 0;
	fm_ovf = 0;
	fm_stop = FM_RUN;
	fm_bn = 0;
	for (;;)
	{
		char c = *fm_fmt;
		if (c == 0 || fm_stop != FM_RUN)
			break;
		fm_fmt++;
		if (c == '%')
			fm_spec();
		else
		{
			fm_c = c;
			fm_put();
		}
	}
	if (fm_mode == FM_CIO && fm_stop != FM_EIO)
		fm_flush();
}

static int fm_result(void)
{
	if (fm_stop == FM_EIO)
		return -(int)fm_st;
	if (fm_stop == FM_EOVF || fm_ovf)
		return -1;
	return fm_cnt;
}

// Each entry point sets up the shared state and calls the engine directly.
// This keeps the call frame small for variadic callers.
static word			fm_size;

static void fm_str(void)
{
	fm_mode = FM_STR;
	fm_lim = fm_size ? fm_size - 1 : 0;
	fm_pos = 0;
	fm_run();
	if (fm_size)
		fm_buf[fm_pos] = 0;
}

static void fm_cio(void)
{
	fm_mode = FM_CIO;
	fm_run();
}

int fmt_vsnprintf(char * buf, word size, const char * fmt, va_list ap)
{
	fm_buf = buf;
	fm_size = size;
	fm_fmt = fmt;
	fm_ap = ap;
	fm_str();
	return fm_result();
}

int fmt_snprintf(char * buf, word size, const char * fmt, ...)
{
	fm_buf = buf;
	fm_size = size;
	fm_fmt = fmt;
	va_start(fm_ap, fmt);
	fm_str();
	return fm_result();
}

int fmt_vcprintf(byte ch, const char * fmt, va_list ap)
{
	fm_ch = ch;
	fm_fmt = fmt;
	fm_ap = ap;
	fm_cio();
	return fm_result();
}

int fmt_cprintf(byte ch, const char * fmt, ...)
{
	fm_ch = ch;
	fm_fmt = fmt;
	va_start(fm_ap, fmt);
	fm_cio();
	return fm_result();
}
