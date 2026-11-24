#include "bitmap.h"
#include "charwin.h"
#include "bitmap_ellipse.h"

static void clipped_span(const ABitmap * bm, int left, int right, int y, byte color)
{
	if (y < 0 || y >= (int)bm->height || right < 0 || left >= (int)bm->width)
		return;
	if (left < 0)
		left = 0;
	if (right >= (int)bm->width)
		right = bm->width - 1;
	abm_fill_rect(bm, left, y, right - left + 1, 1, color);
}

struct Ellipse
{
	const ABitmap * bm;
	int cx, cy;
	byte color;
	bool filled;
};

static void ellipse_spans(const Ellipse * shape, int left, int right, byte row)
{
	clipped_span(shape->bm, shape->cx + left, shape->cx + right,
		shape->cy + row, shape->color);
	if (row)
		clipped_span(shape->bm, shape->cx + left, shape->cx + right,
			shape->cy - row, shape->color);
}

static void ellipse_raster(const ABitmap * bm, int cx, int cy, word rx, byte ry, byte color, bool filled)
{
	if (rx > bm->width || ry > bm->height ||
		cx < -(int)rx || cx >= (int)bm->width + (int)rx ||
		cy < -(int)ry || cy >= (int)bm->height + (int)ry)
		return;

	Ellipse shape = {bm, cx, cy, color, filled};
	long xx = (long)rx * rx, yy = (long)ry * ry;
	long dx = ((long)rx * 2 - 1) * yy;
	long dy = xx;
	long error = 0;
	int x = rx;
	byte row = 0;
	while (row < ry)
	{
		int outer = x;
		ABM_ELLIPSE_NEXT(x, error, dx, dy, xx, yy);
		if (filled || !outer)
			ellipse_spans(&shape, -outer, outer, row);
		else
		{
			int edge = x < outer ? x + 1 : outer;
			ellipse_spans(&shape, -outer, -edge, row);
			ellipse_spans(&shape, edge, outer, row);
		}
		row++;
	}
	ellipse_spans(&shape, -x, x, row);
}

void abm_ellipse(const ABitmap * bm, int cx, int cy, word rx, byte ry, byte color)
{
	ellipse_raster(bm, cx, cy, rx, ry, color, false);
}

void abm_fill_ellipse(const ABitmap * bm, int cx, int cy, word rx, byte ry, byte color)
{
	ellipse_raster(bm, cx, cy, rx, ry, color, true);
}

static byte mark_pitch(const ABitmap * bm)
{
	return (bm->width + 7) >> 3;
}

word abm_flood_size(const ABitmap * bm)
{
	return (word)mark_pitch(bm) * bm->height;
}

struct Flood
{
	const ABitmap * bm;
	byte * marks;
	byte pitch;
	byte before, after;
};

// The scan already has the row address and uses in-bounds coordinates.
static __forceinline byte flood_pixel(const volatile byte * row, word x, byte bits, byte mask)
{
	word bit = bits == 1 ? x : (bits == 2 ? x << 1 : x << 2);
	return (row[bit >> 3] >> (8 - bits - (bit & 7))) & mask;
}

static word flood_span(const Flood * fill, word x, byte y)
{
	const ABitmap * bm = fill->bm;
	const volatile byte * pixels = bm->data + (word)y * bm->pitch;
	byte mask = (1 << bm->bits) - 1;
	word left = x;
	word right = x + 1;
	while (left && flood_pixel(pixels, left - 1, bm->bits, mask) == fill->before)
		left--;
	while (right < bm->width && flood_pixel(pixels, right, bm->bits, mask) == fill->before)
		right++;

	byte * row = fill->marks + (word)y * fill->pitch;
	byte first = left >> 3;
	byte last = (right - 1) >> 3;
	byte start_bits = 0xff << (left & 7);
	byte end_bits = 0xff >> (7 - ((right - 1) & 7));
	if (first == last)
		end_bits &= start_bits;
	else
	{
		row[first] |= start_bits;
		for (byte i = first + 1; i < last; i++)
			row[i] = 0xff;
	}
	row[last] |= end_bits;
	abm_fill_rect(bm, left, y, right - left, 1, fill->after);
	return right;
}

static bool flood_neighbor(const Flood * fill, byte from, byte to)
{
	const ABitmap * bm = fill->bm;
	const byte * source = fill->marks + (word)from * fill->pitch;
	const byte * visited = fill->marks + (word)to * fill->pitch;
	const volatile byte * pixels = bm->data + (word)to * bm->pitch;
	byte mask = (1 << bm->bits) - 1;
	bool changed = false;
	word x = 0;
	while (x < bm->width)
	{
		byte ready = source[x >> 3] & ~visited[x >> 3];
		if (!ready)
			x = (x | 7) + 1;
		else if ((ready & (1 << (x & 7))) && flood_pixel(pixels, x, bm->bits, mask) == fill->before)
		{
			x = flood_span(fill, x, to);
			changed = true;
		}
		else
			x++;
	}
	return changed;
}

bool abm_flood_fill(const ABitmap * bm, word x, byte y, byte color, byte * work, word size)
{
	if (x >= bm->width || y >= bm->height)
		return false;

	byte before = abm_get(bm, x, y);
	color &= (1 << bm->bits) - 1;
	if (before == color)
		return true;

	word needed = abm_flood_size(bm);
	word start = (word)work;
	if (!work || size < needed || start > 0xffffu - (needed - 1))
		return false;
	word end = start + (needed - 1);
	word data = (word)bm->data;
	word extent = (word)(bm->height - 1) * bm->pitch + ((bm->width * bm->bits) >> 3);
	if ((start <= data + (extent - 1) && end >= data) ||
		(start <= (word)bm + (sizeof(ABitmap) - 1) && end >= (word)bm))
		return false;

	for (word i = 0; i < needed; i++)
		work[i] = 0;

	Flood fill = {bm, work, mark_pitch(bm), before, color};
	byte pending[32] = {0};
	flood_span(&fill, x, y);
	pending[y >> 3] = 1 << (y & 7);

	// A row becomes pending when a neighboring row reaches a new span.
	// The mark bitmap distinguishes filled pixels from preexisting ink.
	bool active;
	do
	{
		active = false;
		for (byte row = 0; row < bm->height; row++)
		{
			byte mask = 1 << (row & 7);
			if (pending[row >> 3] & mask)
			{
				pending[row >> 3] &= ~mask;
				active = true;
				if (row && flood_neighbor(&fill, row, row - 1))
					pending[(row - 1) >> 3] |= 1 << ((row - 1) & 7);
				if (row + 1 < bm->height && flood_neighbor(&fill, row, row + 1))
					pending[(row + 1) >> 3] |= 1 << ((row + 1) & 7);
			}
		}
	} while (active);
	return true;
}

void abm_text(const ABitmap * bm, int x, int y, const char * text, const byte * font, byte color)
{
	if (y <= -8 || y >= (int)bm->height)
		return;
	while (*text && x < (int)bm->width)
	{
		if (x > -8)
		{
			byte code = cwin_a2s(*text);
			const byte * glyph = font + ((word)(code & 0x7f) << 3);
			byte inverse = code & 0x80 ? 0xff : 0;
			for (byte row = 0; row < 8; row++)
			{
				int py = y + row;
				if (py < 0 || py >= (int)bm->height)
					continue;
				byte pixels = glyph[row] ^ inverse;
				for (byte col = 0; col < 8 && pixels; col++)
				{
					if (pixels & 0x80)
						abm_put(bm, x + col, py, color);
					pixels <<= 1;
				}
			}
		}
		x += 8;
		text++;
	}
}
