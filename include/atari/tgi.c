#include "../tgi.h"
#include "tgi_driver.h"
#include "cio.h"
#include "charwin.h"
#include "os.h"
#include <stdlib.h>
#include <string.h>

// Use the end of the whole region, including Oscar64's static call frames.
#ifndef TGI_PROGRAM_END
#define TGI_PROGRAM_END 0xbc00u
#endif

#define FILE_CAPACITY 3072
#define IMAGE_CAPACITY 1792

const char tgi_stddrv[] = "atr8.tgi";

static ATgiDriver driver;
static bool installed, drawing;
static byte error, driver_error, color;
static word aspect;
static int pen_x, pen_y;
static ATgiRequest request;

static byte text_font, text_dir;
static word scale_x, scale_y;
static byte mag_x, mag_y;

#define GLYPH_COUNT 95
struct tgi_vectorfont
{
	byte top, bottom, height;
	byte widths[GLYPH_COUNT];
	word offsets[GLYPH_COUNT];
	byte ops[1];
};
static const tgi_vectorfont * vector_font;

static __noinline bool ready(void)
{
	if (!drawing)
		error = installed ? TGI_ERR_INV_MODE : TGI_ERR_NO_DRIVER;
	return drawing;
}

static __noinline void call(byte operation)
{
	request.operation = operation;
	byte status = atgi_call(&request);
	if (status)
		driver_error = status;
}

static void call_a(byte operation, byte a)
{
	request.a = a;
	call(operation);
}

static __noinline const char * disk_path(const char * name, char * buffer)
{
	if (!name || !*name)
		return nullptr;
	word prefix = strchr(name, ':') ? 0 : 2;
	word size = strlen(name);
	if (size > 127 - prefix)
		return nullptr;
	if (prefix)
	{
		buffer[0] = 'D';
		buffer[1] = ':';
	}
	for (word i = 0; i < size; ++i)
	{
		char c = name[i];
		buffer[prefix + i] = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
	}
	buffer[prefix + size] = 0;
	return buffer;
}

static void attach(byte * image, bool loaded)
{
	if (loaded && atgi_attach(&driver))
	{
		installed = true;
		error = driver_error = TGI_ERR_OK;
		pen_x = pen_y = 0;
		byte * header = driver.image;
		aspect = header[14] | (word)header[15] << 8;
	}
	else
	{
		free(image);
		error = TGI_ERR_INV_DRIVER;
	}
}

void tgi_load_driver(const char * name)
{
	if (installed)
	{
		error = TGI_ERR_INSTALLED;
		return;
	}
	char path_buffer[128];
	const char * path = disk_path(name, path_buffer);
	if (!path)
	{
		error = TGI_ERR_CANNOT_LOAD;
		return;
	}
	byte * file = (byte *)malloc(FILE_CAPACITY);
	if (!file)
	{
		error = TGI_ERR_NO_RES;
		return;
	}
	byte * image = (byte *)malloc(IMAGE_CAPACITY);
	if (image)
	{
		ATgiStorage storage = {file, image, FILE_CAPACITY, IMAGE_CAPACITY};
		if (atgi_load_file(&driver, path, &storage))
			attach(image, true);
		else
		{
			free(image);
			error = TGI_ERR_CANNOT_LOAD;
		}
	}
	else
		error = TGI_ERR_NO_RES;
	free(file);
}

void tgi_install(const void * module)
{
	if (installed)
	{
		error = TGI_ERR_INSTALLED;
		return;
	}
	const ATgiModule * embedded = (const ATgiModule *)module;
	if (!embedded || !embedded->file)
	{
		error = TGI_ERR_INV_DRIVER;
		return;
	}
	byte * image = (byte *)malloc(IMAGE_CAPACITY);
	if (!image)
	{
		error = TGI_ERR_NO_RES;
		return;
	}
	attach(image, atgi_load(&driver, embedded->file, embedded->size, image, IMAGE_CAPACITY));
}

void tgi_done(void)
{
	if (drawing)
	{
		atgi_end();
		drawing = false;
	}
}

void tgi_uninstall(void)
{
	if (installed)
	{
		atgi_detach();
		free(driver.image);
		drawing = installed = false;
		color = 0;
		aspect = 0;
	}
	error = driver_error = TGI_ERR_OK;
}

void tgi_unload(void)
{
	tgi_uninstall();
}

__noinline void tgi_init(void)
{
	tgi_done();
	if (!installed)
	{
		error = TGI_ERR_NO_DRIVER;
		return;
	}
	error = atgi_begin(TGI_PROGRAM_END);
	driver_error = TGI_ERR_OK;
	if (error)
		return;
	drawing = true;
	tgi_setviewpage(0);
	tgi_setdrawpage(0);
	const byte * palette = tgi_getdefpalette();
	if (palette)
		tgi_setpalette(palette);
	driver_error = TGI_ERR_OK;
	tgi_setcolor(driver.colors == 1 ? 0 : 1);
	tgi_settextstyle(256, 256, TGI_TEXT_HORIZONTAL, TGI_FONT_BITMAP);
}

unsigned char tgi_geterror(void)
{
	byte result = error ? error : driver_error;
	error = driver_error = TGI_ERR_OK;
	return result;
}

const char * tgi_geterrormsg(byte code)
{
	static const char * const messages[] = {
		"No error", "No driver available", "Cannot load driver or font",
		"Invalid driver", "Graphics mode unavailable", "Invalid argument",
		"Unsupported function", "Invalid font", "Out of resources",
		"Driver already installed"
	};
	return code < sizeof(messages) / sizeof(messages[0]) ? messages[code] : "Unknown error";
}

unsigned tgi_ioctl(byte code, void * data)
{
	if (!installed)
	{
		error = TGI_ERR_NO_DRIVER;
		return 0;
	}
	request.p1 = (word)data;
	call_a(ATGI_CONTROL, code);
	return request.a | (word)request.x << 8;
}

void tgi_clear(void)
{
	if (ready())
		call(ATGI_CLEAR);
}

unsigned tgi_getpagecount(void) { return installed ? driver.pages : 0; }
unsigned char tgi_getcolorcount(void) { return installed ? driver.colors : 0; }
unsigned char tgi_getmaxcolor(void) { return tgi_getcolorcount() - 1; }
unsigned char tgi_getcolor(void) { return color; }
unsigned tgi_getxres(void) { return installed ? driver.width : 0; }
unsigned tgi_getyres(void) { return installed ? driver.height : 0; }
unsigned tgi_getmaxx(void) { return tgi_getxres() - 1; }
unsigned tgi_getmaxy(void) { return tgi_getyres() - 1; }
unsigned tgi_getaspectratio(void) { return aspect; }
void tgi_setaspectratio(unsigned ratio) { aspect = ratio; }

static void set_page(byte page, byte operation)
{
	if (!ready())
		return;
	if (page >= driver.pages)
		error = TGI_ERR_INV_ARG;
	else
		call_a(operation, page);
}

void tgi_setviewpage(byte page) { set_page(page, ATGI_SETVIEWPAGE); }
void tgi_setdrawpage(byte page) { set_page(page, ATGI_SETDRAWPAGE); }

void tgi_setcolor(byte index)
{
	if (!installed)
	{
		error = TGI_ERR_NO_DRIVER;
		return;
	}
	if (driver.colors && index >= driver.colors)
	{
		error = TGI_ERR_INV_ARG;
		return;
	}
	color = index;
	call_a(ATGI_SETCOLOR, index);
}

void tgi_setpalette(const byte * palette)
{
	if (!installed)
		error = TGI_ERR_NO_DRIVER;
	else if (!palette)
		error = TGI_ERR_INV_ARG;
	else
	{
		request.p1 = (word)palette;
		call(ATGI_SETPALETTE);
	}
}

static const byte * get_palette(byte operation)
{
	if (!installed)
	{
		error = TGI_ERR_NO_DRIVER;
		return nullptr;
	}
	call(operation);
	return (const byte *)(request.a | (word)request.x << 8);
}

const unsigned char * tgi_getpalette(void) { return get_palette(ATGI_GETPALETTE); }
const unsigned char * tgi_getdefpalette(void) { return get_palette(ATGI_GETDEFPALETTE); }

static bool in_bounds(int x, int y)
{
	return (word)x < driver.width && (word)y < driver.height;
}

static void pixel(int x, int y, byte operation)
{
	request.p1 = x;
	request.p2 = y;
	call(operation);
}

__noinline void tgi_setpixel(int x, int y)
{
	if (ready() && in_bounds(x, y))
		pixel(x, y, ATGI_SETPIXEL);
}

__noinline unsigned char tgi_getpixel(int x, int y)
{
	if (!ready() || !in_bounds(x, y))
		return 0;
	pixel(x, y, ATGI_GETPIXEL);
	return request.a;
}

void tgi_gotoxy(int x, int y)
{
	pen_x = x;
	pen_y = y;
}

static __noinline void draw_rect(int x1, int y1, int x2, int y2, byte operation)
{
	request.p1 = x1;
	request.p2 = y1;
	request.p3 = x2;
	request.p4 = y2;
	call(operation);
}

static __noinline bool bar_axis(int * first, int * last, int limit)
{
	int lo = *first, hi = *last;
	if (lo > hi)
	{
		int swap = lo;
		lo = hi;
		hi = swap;
	}
	if (hi < 0 || lo >= limit)
		return false;
	*first = lo < 0 ? 0 : lo;
	*last = hi >= limit ? limit - 1 : hi;
	return true;
}

__noinline void tgi_bar(int x1, int y1, int x2, int y2)
{
	if (ready() && bar_axis(&x1, &x2, driver.width) && bar_axis(&y1, &y2, driver.height))
		draw_rect(x1, y1, x2, y2, ATGI_BAR);
}

static byte outcode(int x, int y)
{
	return (x < 0 ? 1 : x >= (int)driver.width ? 2 : 0) |
		(y < 0 ? 4 : y >= (int)driver.height ? 8 : 0);
}

static int interpolate(int a, int b, word distance, word span)
{
	// The span and delta can each be 65535 for signed 16-bit coordinates.
	if (a <= b)
		return (word)a + (word)((dword)((word)b - (word)a) * distance / span);
	return (word)a - (word)((dword)((word)a - (word)b) * distance / span);
}

static void clip_start(int * x1, int * y1, int x2, int y2, byte outside)
{
	int x = *x1, y = *y1;
	if (outside & 3)
	{
		int edge = (outside & 1) ? 0 : driver.width - 1;
		*y1 = x < edge ? interpolate(y, y2, (word)edge - x, (word)x2 - x)
			: interpolate(y, y2, (word)x - edge, (word)x - x2);
		*x1 = edge;
	}
	else
	{
		int edge = (outside & 4) ? 0 : driver.height - 1;
		*x1 = y < edge ? interpolate(x, x2, (word)edge - y, (word)y2 - y)
			: interpolate(x, x2, (word)y - edge, (word)y - y2);
		*y1 = edge;
	}
}

static __noinline void clipped_line(int x1, int y1, int x2, int y2)
{
	byte start = outcode(x1, y1), end = outcode(x2, y2);
	while (start | end)
	{
		if (start & end)
			return;
		if (start)
		{
			clip_start(&x1, &y1, x2, y2, start);
			start = outcode(x1, y1);
		}
		else
		{
			clip_start(&x2, &y2, x1, y1, end);
			end = outcode(x2, y2);
		}
	}
	draw_rect(x1, y1, x2, y2, ATGI_LINE);
}

void tgi_line(int x1, int y1, int x2, int y2)
{
	tgi_gotoxy(x2, y2);
	if (ready())
		clipped_line(x1, y1, x2, y2);
}

void tgi_lineto(int x, int y)
{
	tgi_line(pen_x, pen_y, x, y);
}

int tgi_imulround(int rhs, int lhs)
{
	long product = (long)rhs * lhs;
	return product < 0 ? -(int)((-product + 128) >> 8) : (int)((product + 128) >> 8);
}

static __noinline int degree_sine(unsigned angle)
{
	// Round sin(degrees) * 256. The final four values are 256.
	static const byte quarter[87] = {
		0, 4, 9, 13, 18, 22, 27, 31, 36, 40, 44, 49, 53, 58, 62, 66,
		71, 75, 79, 83, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124,
		128, 132, 136, 139, 143, 147, 150, 154, 158, 161, 165, 168, 171,
		175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 204, 207, 210,
		212, 215, 217, 219, 222, 224, 226, 228, 230, 232, 234, 236, 237,
		239, 241, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 253,
		254, 254, 255, 255, 255
	};
	if (angle >= 360)
		angle -= 360;
	bool negative = angle >= 180;
	if (negative)
		angle -= 180;
	if (angle > 90)
		angle = 180 - angle;
	int value = 256;
	if (angle < sizeof(quarter))
		value = quarter[angle];
	return negative ? -value : value;
}

static int add_coordinate(int origin, int delta)
{
	long result = (long)origin + delta;
	if (result > 32767)
		return 32767;
	if (result < -32768L)
		return -32767 - 1;
	return (int)result;
}

static __noinline void arc(int x, int y, byte rx, byte ry, word sa, word ea, bool close)
{
	if (!ready() || sa > ea)
		return;
	if (ea > 360)
	{
		error = TGI_ERR_INV_ARG;
		return;
	}
	int start_x = add_coordinate(x, tgi_imulround(rx, degree_sine(sa + 90)));
	int start_y = add_coordinate(y, -tgi_imulround(ry, degree_sine(sa)));
	int prev_x = start_x, prev_y = start_y;
	word step = (word)rx + ry >= 25 ? 12 : 24;
	do
	{
		sa += step;
		if (sa > ea)
			sa = ea;
		int next_x = add_coordinate(x, tgi_imulround(rx, degree_sine(sa + 90)));
		int next_y = add_coordinate(y, -tgi_imulround(ry, degree_sine(sa)));
		clipped_line(prev_x, prev_y, next_x, next_y);
		prev_x = next_x;
		prev_y = next_y;
	} while (sa < ea);
	if (close)
	{
		clipped_line(x, y, start_x, start_y);
		clipped_line(x, y, prev_x, prev_y);
	}
	tgi_gotoxy(prev_x, prev_y);
}

void tgi_arc(int x, int y, byte rx, byte ry, word sa, word ea)
{
	arc(x, y, rx, ry, sa, ea, false);
}

void tgi_pieslice(int x, int y, byte rx, byte ry, word sa, word ea)
{
	arc(x, y, rx, ry, sa, ea, true);
}

void tgi_ellipse(int x, int y, byte rx, byte ry)
{
	tgi_arc(x, y, rx, ry, 0, 360);
}

void tgi_circle(int x, int y, byte radius)
{
	tgi_ellipse(x, y, radius, radius);
}

static bool valid_font(const tgi_vectorfont * font, word size)
{
	const byte * raw = (const byte *)font;
	word base = (const byte *)font->ops - raw;
	if (size <= base)
		return false;
	word remaining = size - base;
	for (byte glyph = 0; glyph < GLYPH_COUNT; ++glyph)
	{
		word offset = font->offsets[glyph];
		for (;;)
		{
			if (offset >= remaining || remaining - offset < 2)
				return false;
			if (font->ops[offset] & 0x80)
				break;
			offset += 2;
		}
	}
	return true;
}

const tgi_vectorfont * tgi_load_vectorfont(const char * name)
{
	char path_buffer[128];
	const char * path = disk_path(name, path_buffer);
	byte ch = cio_free();
	error = TGI_ERR_CANNOT_LOAD;
	if (!path || ch == 255 || cio_open(ch, path, CIO_READ, 0) != CIO_OK)
		return nullptr;

	byte header[6];
	word done;
	byte status = cio_read(ch, header, sizeof(header), &done);
	tgi_vectorfont * font = nullptr;
	if (!cio_failed(status) && done == sizeof(header))
	{
		word size = header[4] | (word)header[5] << 8;
		if (header[0] != 'T' || header[1] != 'C' || header[2] != 'H' ||
			header[3] || size < sizeof(tgi_vectorfont) || size > 0x7fffu)
			error = TGI_ERR_INV_FONT;
		else
		{
			font = (tgi_vectorfont *)malloc(size);
			if (!font)
				error = TGI_ERR_NO_RES;
			else
			{
				status = cio_read(ch, font, size, &done);
				if (!cio_failed(status) && done == size)
					error = valid_font(font, size) ? TGI_ERR_OK : TGI_ERR_INV_FONT;
			}
		}
	}
	if (cio_close(ch) != CIO_OK && !error)
		error = TGI_ERR_CANNOT_LOAD;
	if (error && font)
	{
		free(font);
		font = nullptr;
	}
	return font;
}

void tgi_install_vectorfont(const tgi_vectorfont * font)
{
	vector_font = font;
}

void tgi_free_vectorfont(const tgi_vectorfont * font)
{
	if (font == vector_font)
		vector_font = nullptr;
	if (font)
		free((void *)font);
}

static void bitmap_style(void)
{
	if (installed && text_font == TGI_FONT_BITMAP)
	{
		request.x = mag_x;
		request.y = mag_y;
		call_a(ATGI_TEXTSTYLE, text_dir);
	}
}

void tgi_settextdir(byte dir)
{
	if (dir > TGI_TEXT_VERTICAL)
		error = TGI_ERR_INV_ARG;
	else
	{
		text_dir = dir;
		bitmap_style();
	}
}

void tgi_settextscale(word width, word height)
{
	scale_x = width;
	scale_y = height;
	// Atari modules accept whole pixel magnifications in 8-bit registers.
	mag_x = (width >> 8) + ((width & 255) >= 128 && width < 0xff80u);
	mag_y = (height >> 8) + ((height & 255) >= 128 && height < 0xff80u);
	bitmap_style();
}

void tgi_settextstyle(word width, word height, byte dir, byte font)
{
	if (dir > TGI_TEXT_VERTICAL || font > TGI_FONT_VECTOR)
	{
		error = TGI_ERR_INV_ARG;
		return;
	}
	text_font = font;
	text_dir = dir;
	tgi_settextscale(width, height);
}

static byte glyph_index(byte c)
{
	c &= 127;
	return c < 32 || c > 126 ? '?' - 32 : c - 32;
}

static word scale_size(word unscaled, word scale)
{
	return (word)(((dword)unscaled * scale + 128) >> 8);
}

#pragma optimize(push, noconstparams)
__noinline unsigned tgi_gettextwidth(const char * s)
{
	if (!s)
		return 0;
	if (text_font == TGI_FONT_BITMAP)
		return installed ? strlen(s) * driver.image[12] * mag_x : 0;
	if (!vector_font)
		return 0;
	word width = 0;
	while (*s)
	{
		width += vector_font->widths[glyph_index(*s)];
		++s;
	}
	return scale_size(width, scale_x);
}

unsigned tgi_gettextheight(const char * s)
{
	if (text_font == TGI_FONT_BITMAP)
		return installed ? (word)driver.image[13] * mag_y : 0;
	return vector_font ? scale_size(vector_font->height, scale_y) : 0;
}

static int glyph_delta(byte op, word scale)
{
	int value = op & 127;
	if (value & 64)
		value -= 128;
	long product = (long)value * scale;
	return product < 0 ? -(int)((-product + 128) >> 8) : (int)((product + 128) >> 8);
}

static void vector_glyph(byte glyph)
{
	const byte * ops = vector_font->ops + vector_font->offsets[glyph];
	int x = pen_x, y = pen_y;
	byte op_x, op_y;
	do
	{
		op_x = *ops++;
		op_y = *ops++;
		int dx = glyph_delta(op_x, scale_x);
		int dy = glyph_delta(op_y, scale_y);
		int nx = add_coordinate(pen_x, text_dir == TGI_TEXT_VERTICAL ? -dy : dx);
		int ny = add_coordinate(pen_y, text_dir == TGI_TEXT_VERTICAL ? -dx : -dy);
		if (op_y & 0x80)
			clipped_line(x, y, nx, ny);
		x = nx;
		y = ny;
	} while (!(op_x & 0x80));
}

static void advance_pen(word width)
{
	if (text_dir == TGI_TEXT_VERTICAL)
		pen_y = (word)pen_y - width;
	else
		pen_x = (word)pen_x + width;
}

static __noinline const byte * bitmap_font(void)
{
	// Materialize the volatile CHBAS byte before shifting it into a pointer.
	volatile byte page = os_chbas;
	return (const byte *)((word)page << 8);
}

static __noinline void clipped_glyph(byte ch, int x, int y)
{
	byte code = cwin_a2s(ch);
	const byte * glyph = bitmap_font() + ((word)(code & 127) << 3);
	byte inverse = code & 128 ? 255 : 0;
	int row_offset = -8 * (int)mag_y;
	for (byte row = 0; row < 8; ++row)
	{
		byte bits = glyph[row] ^ inverse;
		int column_offset = 0;
		for (byte column = 0; column < 8; ++column)
		{
			if (bits & 128)
			{
				if (text_dir == TGI_TEXT_VERTICAL)
					tgi_bar(x + row_offset, y - column_offset,
						x + row_offset + mag_y - 1, y - column_offset + mag_x - 1);
				else
					tgi_bar(x + column_offset, y + row_offset,
						x + column_offset + mag_x - 1, y + row_offset + mag_y - 1);
			}
			bits <<= 1;
			column_offset += mag_x;
		}
		row_offset += mag_y;
	}
}

static __noinline void clipped_bitmap_text(const char * s, word cell_width, word cell_height)
{
	// Atari TGI bitmap glyphs are eight rows of the current OS font.
	if (driver.image[12] != 8 || driver.image[13] != 8)
		return;
	long baseline = text_dir == TGI_TEXT_VERTICAL ? pen_y : pen_x;
	while (*s)
	{
		if (text_dir == TGI_TEXT_VERTICAL)
		{
			if (baseline + mag_x <= 0)
				break;
			if (baseline - cell_width + mag_x < driver.height &&
				pen_x > 0 && (long)pen_x - cell_height < driver.width)
				clipped_glyph(*s, pen_x, (int)baseline);
			baseline -= cell_width;
		}
		else
		{
			if (baseline >= driver.width)
				break;
			if (baseline + cell_width > 0 && pen_y > 0 &&
				(long)pen_y - cell_height < driver.height)
				clipped_glyph(*s, (int)baseline, pen_y);
			baseline += cell_width;
		}
		++s;
	}
}

static void bitmap_text(const char * s)
{
	word cell_width = (word)driver.image[12] * mag_x;
	word cell_height = (word)driver.image[13] * mag_y;
	dword width = (dword)strlen(s) * cell_width;
	if (width && cell_height)
	{
		// The Atari OUTTEXT vector does byte arithmetic on Y. Only pass a
		// run that fits on screen and clip partial glyphs before invoking BAR.
		bool fits;
		if (text_dir == TGI_TEXT_VERTICAL)
			fits = pen_x >= (int)cell_height && pen_x <= (int)driver.width &&
				pen_y >= 0 && (word)pen_y + (word)mag_x <= driver.height &&
				width <= (word)pen_y + (word)mag_x;
		else
			fits = pen_y >= (int)cell_height && pen_y <= (int)driver.height &&
				pen_x >= 0 && pen_x < (int)driver.width &&
				width <= (word)(driver.width - pen_x);

		if (fits)
		{
			request.p1 = pen_x;
			request.p2 = pen_y;
			request.p3 = (word)s;
			call(ATGI_OUTTEXT);
		}
		else
			clipped_bitmap_text(s, cell_width, cell_height);
	}
	advance_pen((word)width);
}

__noinline void tgi_outtext(const char * s)
{
	if (!ready())
		return;
	if (!s)
	{
		error = TGI_ERR_INV_ARG;
		return;
	}
	if (text_font == TGI_FONT_BITMAP)
		bitmap_text(s);
	else if (vector_font)
	{
		while (*s)
		{
			byte glyph = glyph_index(*s);
			++s;
			vector_glyph(glyph);
			advance_pen(scale_size(vector_font->widths[glyph], scale_x));
		}
	}
}

#pragma optimize(pop)

void tgi_outtextxy(int x, int y, const char * s)
{
	tgi_gotoxy(x, y);
	tgi_outtext(s);
}
