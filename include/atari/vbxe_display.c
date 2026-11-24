#include "vbxe_display.h"
#include "bitmap_canvas.h"
#include "charwin.h"

static bool text_mode(const AVbxeDisplay * display)
{
	return display->mode >= VBXE_DISPLAY_TEXT;
}

static byte condensed_rows(byte mode)
{
	if (mode == VBXE_DISPLAY_TEXT_80X32)
		return 32;
	return mode == VBXE_DISPLAY_TEXT_80X30 ? 30 : 0;
}

word vbxe_display_width(const AVbxeDisplay * display)
{
	if (!display || display->mode > VBXE_DISPLAY_TEXT_80X30 || display->width > VBXE_DISPLAY_WIDE)
		return 0;
	byte rows = condensed_rows(display->mode);
	if (rows)
		return display->width == VBXE_DISPLAY_NORMAL && display->height == rows * 7 ? 80 : 0;
	static const word widths[4][3] =
	{
		{128, 160, 168},
		{256, 320, 336},
		{512, 640, 672},
		{64, 80, 84}
	};
	return widths[display->mode][display->width];
}

static byte text_rows(const AVbxeDisplay * display)
{
	byte rows = condensed_rows(display->mode);
	return rows ? rows : (display->height + 7u) >> 3;
}

static bool intersects(unsigned long a, unsigned long n, unsigned long b, unsigned long m)
{
	return a < b + m && b < a + n;
}

static __noinline word surface_width(const AVbxeDisplay * display)
{
	word width = vbxe_display_width(display);
	if (!width || !display->board || !display->height || display->height > 240 ||
		display->pixels >= VBXE_VRAM_SIZE)
		return 0;
	byte rows = display->height;
	word bytes = width;
	if (text_mode(display))
	{
		bytes = width << 1;
		rows = text_rows(display);
	}
	else if (display->mode == VBXE_DISPLAY_HIGH)
		bytes >>= 1;
	if (display->pitch < bytes || display->pitch > 4095)
		return 0;
	unsigned long extent = (unsigned long)(rows - 1) * display->pitch + bytes;
	return extent <= VBXE_VRAM_SIZE - display->pixels ? width : 0;
}

byte vbxe_display_rows(const AVbxeDisplay * display)
{
	return surface_width(display) && text_mode(display) ? text_rows(display) : 0;
}

static void pack_address(byte * dest, unsigned long address)
{
	dest[0] = address;
	dest[1] = address >> 8;
	dest[2] = address >> 16;
}

static bool map_extent(const AVbxeDisplay * display, const AVbxeColorMap * map, unsigned long * extent)
{
	if (!map->cell_width || map->cell_width > 32 || !map->cell_height || map->cell_height > 32 ||
		map->scroll_x >= map->cell_width || map->scroll_y >= map->cell_height ||
		map->pitch > 4095 || map->data >= VBXE_VRAM_SIZE)
		return false;
	static const word widths[3] = {256, 320, 336};
	word columns = (widths[display->width] + map->scroll_x + map->cell_width - 1) / map->cell_width;
	if (columns > 43)
		columns = 43;
	word bytes = columns << 2;
	if (map->pitch < bytes)
		return false;
	word rows = ((word)display->height + map->scroll_y + map->cell_height - 1) / map->cell_height;
	*extent = (unsigned long)(rows - 1) * map->pitch + bytes;
	return *extent <= VBXE_VRAM_SIZE - map->data;
}

#pragma optimize(push, 0, noinline, noconstparams)
static __noinline bool present(const AVbxeDisplay * request, const byte * xdl, word length)
{
	// Reload these after binding the MEMAC call's first parameters. On
	// Oscar64 the incoming and outgoing register argument slots can overlap.
	const AVbxeDisplay * volatile display = request;
	const byte * volatile data = xdl;
	volatile word size = length;
	if (!vbxe_write(display->board, display->list, data, size))
		return false;
	return vbxe_xdl(display->board, display->list, VBXE_XDL_ENABLED | display->control);
}

static __noinline bool show_condensed(const AVbxeDisplay * display, const byte * header, byte length)
{
	byte list[VBXE_DISPLAY_80X32_MAP_XDL_SIZE];
	for (byte i = 0; i < length; i++)
		list[i] = header[i];
	byte * entry = list + length;
	unsigned long pixels = display->pixels;
	word pitch = display->pitch;
	byte rows = condensed_rows(display->mode);
	for (byte row = 1; row < rows; row++, entry += 8)
	{
		pixels += pitch;
		entry[0] = 0x60; // Repeat and load the next row address.
		entry[1] = 0;
		entry[2] = 6;
		pack_address(entry + 3, pixels);
		entry[6] = pitch;
		entry[7] = pitch >> 8;
	}
	entry[0] = 4;
	entry[1] = 0x80;
	return present(display, list, entry + 2 - list);
}

static __noinline bool show(const AVbxeDisplay * request, const AVbxeColorMap * map_request)
{
	const AVbxeDisplay * volatile display = request;
	// Preserve the optional pointer across Oscar64's call specialization.
	const AVbxeColorMap * volatile map = map_request;
	word width = surface_width(display);
	word list_size = VBXE_DISPLAY_XDL_SIZE;
	if (map)
		list_size = VBXE_DISPLAY_MAP_XDL_SIZE;
	byte condensed = width ? condensed_rows(display->mode) : 0;
	if (condensed)
		list_size += (word)(condensed - 1) * 8;
	if (!width || (word)display->top + display->height > 240 ||
		display->list > VBXE_VRAM_SIZE - list_size ||
		display->palette >= 4 || display->playfield_palette >= 4 ||
		(display->control & (byte)~(VBXE_EXTENDED_COLOR | VBXE_OPAQUE | VBXE_TRANSPARENT_15)))
		return false;
	bool text = text_mode(display);
	byte rows = text ? text_rows(display) : display->height;
	word row_bytes = text ? width << 1 : width;
	if (display->mode == VBXE_DISPLAY_HIGH)
		row_bytes >>= 1;
	unsigned long extent = (unsigned long)(rows - 1) * display->pitch + row_bytes;
	unsigned long list = display->list, pixels = display->pixels, charset = display->charset;
	if (intersects(list, list_size, pixels, extent))
		return false;
	if (text && ((charset & 2047ul) || charset > VBXE_VRAM_SIZE - 2048 ||
		intersects(charset, 2048, list, list_size) || intersects(charset, 2048, pixels, extent)))
		return false;

	if (map)
	{
		unsigned long size;
		if (!map_extent(display, map, &size) || intersects(map->data, size, list, list_size) ||
			intersects(map->data, size, pixels, extent) || (text && intersects(map->data, size, charset, 2048)))
			return false;
	}

	byte xdl[VBXE_DISPLAY_MAP_XDL_SIZE];
	byte at = 0;
	if (display->top)
	{
		xdl[at++] = 0x24;
		xdl[at++] = 0;
		xdl[at++] = display->top - 1;
	}
	static const byte modes[4] = {0x28, 0x08, 0x18, 0x09};
	byte mode = text ? 0x61 : 0x62;
	byte options = modes[text ? VBXE_DISPLAY_TEXT : display->mode];
	if (map)
	{
		mode |= 8;
		options |= 6;
	}
	xdl[at++] = mode;
	xdl[at++] = options;
	xdl[at++] = condensed ? 6 : display->height - 1;
	pack_address(xdl + at, pixels);
	at += 3;
	xdl[at++] = display->pitch;
	xdl[at++] = display->pitch >> 8;
	if (text)
		xdl[at++] = charset >> 11;
	if (map)
	{
		pack_address(xdl + at, map->data);
		at += 3;
		xdl[at++] = map->pitch;
		xdl[at++] = map->pitch >> 8;
		xdl[at++] = map->scroll_x;
		xdl[at++] = map->scroll_y;
		xdl[at++] = map->cell_width - 1;
		xdl[at++] = map->cell_height - 1;
	}
	xdl[at++] = display->width | (display->palette << 4) | (display->playfield_palette << 6);
	xdl[at++] = display->priority;
	if (condensed)
		return show_condensed(display, xdl, at);
	xdl[at++] = 4;
	xdl[at++] = 0x80;
	return present(display, xdl, at);
}
#pragma optimize(pop)

bool vbxe_display_show(const AVbxeDisplay * display)
{
	return show(display, 0);
}

bool vbxe_display_show_map(const AVbxeDisplay * display, const AVbxeColorMap * map)
{
	return map && show(display, map);
}

bool vbxe_display_map_priority(const AVbxe * board, byte entry, byte priority)
{
	if (!board || entry >= 4)
		return false;
	board->regs[0x55 + entry] = priority;
	return true;
}

static unsigned long row_address(const AVbxeDisplay * display, byte y)
{
	return display->pixels + (unsigned long)y * display->pitch;
}

static bool graphic_area(const AVbxeDisplay * display, word x, byte y)
{
	return x < surface_width(display) && !text_mode(display) && y < display->height;
}

// Keep MEMAC-A reads and writes as calls on Oscar64.
#pragma optimize(push, 0, noinline, noconstparams)
__noinline bool vbxe_display_get(const AVbxeDisplay * display, word x, byte y, byte * color)
{
	byte * volatile result = color;
	if (!result || !graphic_area(display, x, y))
		return false;
	bool packed = display->mode == VBXE_DISPLAY_HIGH;
	unsigned long addr = row_address(display, y);
	volatile byte sample;
	if (!vbxe_read(display->board, addr + (packed ? x >> 1 : x), (byte *)&sample, 1))
		return false;
	byte value = sample;
	if (packed)
		value = x & 1 ? value & 15 : value >> 4;
	*result = value;
	return true;
}

static __noinline bool store_pixel(const AVbxe * board, unsigned long addr, word x, bool packed, byte color)
{
	if (!packed)
		return vbxe_fill(board, addr + x, color, 1);
	// Oscar64 can reuse the incoming color slot for a MEMAC call.
	volatile byte ink = color;
	byte pair;
	addr += x >> 1;
	if (!vbxe_read(board, addr, &pair, 1))
		return false;
	if (x & 1)
		pair = (pair & 0xf0) | ink;
	else
		pair = (pair & 0x0f) | (ink << 4);
	return vbxe_write(board, addr, &pair, 1);
}

#pragma optimize(pop)

// Preserve the drawing call boundaries across caller optimization levels.
#pragma optimize(push, 2, noinline, noconstparams)
__noinline bool vbxe_display_put(const AVbxeDisplay * display, word x, byte y, byte color)
{
	if (!graphic_area(display, x, y))
		return false;
	bool packed = display->mode == VBXE_DISPLAY_HIGH;
	return (!packed || color < 16) && store_pixel(display->board, row_address(display, y), x, packed, color);
}

static __noinline bool span(const AVbxeDisplay * display, unsigned long addr, word x0, word x1, byte color)
{
	const AVbxe * board = display->board;
	if (display->mode != VBXE_DISPLAY_HIGH)
		return vbxe_fill(board, addr + x0, color, x1 - x0 + 1);
	if (x0 & 1)
	{
		if (!store_pixel(board, addr, x0, true, color))
			return false;
		x0++;
	}
	if (x0 > x1)
		return true;
	if (!(x1 & 1))
	{
		if (!store_pixel(board, addr, x1, true, color))
			return false;
		if (x0 == x1)
			return true;
		x1--;
	}
	return vbxe_fill(board, addr + (x0 >> 1), (color << 4) | color, (x1 - x0 + 1) >> 1);
}

__noinline bool vbxe_display_rect(const AVbxeDisplay * display, word x0, byte y0, word x1, byte y1, byte color)
{
	if (x0 > x1 || y0 > y1 || !graphic_area(display, x1, y1) ||
		(display->mode == VBXE_DISPLAY_HIGH && color >= 16))
		return false;
	unsigned long addr = row_address(display, y0);
	for (byte y = y0; y <= y1; y++, addr += display->pitch)
		if (!span(display, addr, x0, x1, color))
			return false;
	return true;
}

__noinline bool vbxe_display_line(const AVbxeDisplay * display, word x0, byte y0, word x1, byte y1, byte color)
{
	if (!graphic_area(display, x0, y0) || !graphic_area(display, x1, y1))
		return false;
	bool packed = display->mode == VBXE_DISPLAY_HIGH;
	if (packed && color >= 16)
		return false;
	int dx = x0 <= x1 ? x1 - x0 : x0 - x1;
	int dy = y0 <= y1 ? y1 - y0 : y0 - y1;
	int sx = x0 <= x1 ? 1 : -1;
	int sy = y0 <= y1 ? 1 : -1;
	int error = dx - dy;
	unsigned long addr = row_address(display, y0);
	for (;;)
	{
		if (!store_pixel(display->board, addr, x0, packed, color))
			return false;
		if (x0 == x1 && y0 == y1)
			return true;
		int doubled = 2 * error;
		if (doubled > -dy)
		{
			error -= dy;
			x0 += sx;
		}
		if (doubled < dx)
		{
			error += dx;
			y0 += sy;
			if (sy < 0)
				addr -= display->pitch;
			else
				addr += display->pitch;
		}
	}
}

struct TextRun
{
	const AVbxeDisplay * display;
	const AVbxeBitmapText * ink;
	int x, limit;
	byte height;
};

static __noinline bool draw_glyph(const TextRun * run, byte code, byte width)
{
	const AVbxeBitmapText * ink = run->ink;
	const byte * glyph = ink->font->glyphs + ((word)(code & 0x7f) << 3);
	byte inverse = ((code >> 7) ^ !!(ink->flags & VBXE_INK_INVERSE)) ? 0xff : 0;
	int first = run->x < 0 ? -run->x : 0;
	int end = run->limit - run->x;
	if (end > width)
		end = width;
	for (byte row = 0; row < run->height; row++)
	{
		int y = ink->y + row;
		if (y < 0 || y >= run->display->height)
			continue;
		byte bits = glyph[row] ^ inverse;
		if ((ink->flags & VBXE_INK_UNDERLINE) && row + 1 == run->height)
			bits = 0xff;
		unsigned long addr = row_address(run->display, y);
		for (byte col = first; col < end;)
		{
			byte start = col;
			bool set = bits & (0x80 >> col);
			do
				col++;
			while (col < end && !!(bits & (0x80 >> col)) == set);
			if ((set || (ink->flags & VBXE_INK_OPAQUE)) &&
				!span(run->display, addr, run->x + start, run->x + col - 1,
					set ? ink->foreground : ink->background))
				return false;
		}
	}
	return true;
}

__noinline bool vbxe_display_text(const AVbxeDisplay * display, const AVbxeBitmapText * ink, const char * text)
{
	word width = surface_width(display);
	if (!width || text_mode(display) || !ink || !ink->font || !ink->font->glyphs || !text ||
		(ink->flags & 0xf0) || (display->mode == VBXE_DISPLAY_HIGH &&
		(ink->foreground >= 16 || ((ink->flags & VBXE_INK_OPAQUE) && ink->background >= 16))))
		return false;
	TextRun run = {display, ink, ink->x, (int)width, (byte)((ink->flags & VBXE_INK_CONDENSED) ? 7 : 8)};
	if (ink->y <= -(int)run.height || ink->y >= display->height)
		return true;
	const byte * widths = ink->font->widths;
	while (run.x < run.limit && *text)
	{
		byte code = cwin_a2s(*text++);
		byte count = widths ? widths[code & 0x7f] : 8;
		if (count > 8)
			count = 8;
		if (run.x + count > 0 && !draw_glyph(&run, code, count))
			return false;
		run.x += (word)count + ink->font->spacing;
	}
	return true;
}

static bool text_area(const AVbxeDisplay * display, byte x, byte y, byte count)
{
	word width = surface_width(display);
	return width && text_mode(display) && x < width && count <= width - x && y < text_rows(display);
}

__noinline bool vbxe_display_read_text(const AVbxeDisplay * display, byte x, byte y, AVbxeTextCell * cells, byte count)
{
	return text_area(display, x, y, count) &&
		vbxe_read(display->board, row_address(display, y) + ((word)x << 1), (byte *)cells, (word)count << 1);
}

__noinline bool vbxe_display_write_text(const AVbxeDisplay * display, byte x, byte y, const AVbxeTextCell * cells, byte count)
{
	return text_area(display, x, y, count) &&
		vbxe_write(display->board, row_address(display, y) + ((word)x << 1), (const byte *)cells, (word)count << 1);
}
#pragma optimize(pop)


#pragma optimize(push, 0, noinline, noconstparams)
bool vbxe_display_copy(const AVbxeDisplay * display, const AVbxeDisplayCopy * request)
{
	const AVbxeDisplayCopy * volatile copy = request;
	word width = surface_width(display);
	if (!width || !copy || display->pitch > 2047 || copy->command > VBXE_VRAM_SIZE - VBXE_BCB_SIZE)
		return false;
	bool text = text_mode(display);
	bool packed = display->mode == VBXE_DISPLAY_HIGH;
	byte rows = text ? text_rows(display) : display->height;
	word sx = copy->source_x, dx = copy->dest_x, count = copy->width;
	byte sy = copy->source_y, dy = copy->dest_y, height = copy->height;
	if (!count || !height || sx >= width || dx >= width || sy >= rows || dy >= rows ||
		count > width - sx || count > width - dx || height > rows - sy || height > rows - dy ||
		((text || packed) && copy->transparent))
		return false;
	if (packed)
	{
		if ((sx | dx | count) & 1)
			return false;
		sx >>= 1;
		dx >>= 1;
		count >>= 1;
		width >>= 1;
	}
	else if (text)
	{
		sx <<= 1;
		dx <<= 1;
		count <<= 1;
		width <<= 1;
	}

	unsigned long extent = (unsigned long)(rows - 1) * display->pitch + width;
	if (intersects(copy->command, VBXE_BCB_SIZE, display->pixels, extent))
		return false;

	int pitch = display->pitch;
	if (dy > sy && dy - sy < height)
	{
		sy += height - 1;
		dy += height - 1;
		pitch = -pitch;
	}
	signed char step = 1;
	if (dx > sx && dx - sx < count)
	{
		sx += count - 1;
		dx += count - 1;
		step = -1;
	}

	AVbxeBlit blit = {row_address(display, sy) + sx, row_address(display, dy) + dx,
		pitch, pitch, step, step, count, height, 255, 0, 0, 0, 0, copy->transparent ? 1 : 0};
	return vbxe_blit(display->board, copy->command, &blit);
}
#pragma optimize(pop)
