#include "bitmap_canvas.h"

static word distance(int a, int b)
{
	return a < b ? (word)b - (word)a : (word)a - (word)b;
}

// Keep the raster routines separate to fit smaller code regions at -O3.
static __noinline void polygon_edge(const ABitmap * bm, const ABitmapPoint * a, const ABitmapPoint * b, byte color)
{
	word dx = distance(a->x, b->x);
	word dy = distance(a->y, b->y);
	bool steep = dy > dx;
	int first = steep ? a->y : a->x;
	int last = steep ? b->y : b->x;
	int from = steep ? a->x : a->y;
	int to = steep ? b->x : b->y;
	int limit = steep ? (int)bm->height : (int)bm->width;
	word minor_limit = steep ? bm->width : bm->height;

	if (first > last)
	{
		int swap = first;
		first = last;
		last = swap;
		swap = from;
		from = to;
		to = swap;
	}

	if (last < 0 || first >= limit || (from < 0 && to < 0) ||
		(from >= (int)minor_limit && to >= (int)minor_limit))
		return;

	word major = steep ? dy : dx;
	word minor = steep ? dx : dy;
	int start = first < 0 ? 0 : first;
	int end = last < limit ? last : limit - 1;
	if (!minor)
	{
		if (steep)
			abm_fill_rect(bm, from, start, 1, end - start + 1, color);
		else
			abm_fill_rect(bm, start, from, end - start + 1, 1, color);
		return;
	}

	int direction = from > to ? -1 : 1;
	word steps = (word)start - (word)first;
	word value = from;
	word error = major >> 1;
	// Seed Bresenham at the first visible major coordinate.
	if (steps)
	{
		unsigned long advance = (unsigned long)steps * minor + error;
		word offset = advance / major;
		error = advance - (unsigned long)offset * major;
		value = direction < 0 ? (word)from - offset : (word)from + offset;
	}
	word threshold = major - minor;
	for (int pos = start; pos <= end; pos++)
	{
		if (value < minor_limit)
		{
			if (steep)
				abm_put(bm, value, pos, color);
			else
				abm_put(bm, pos, value, color);
		}
		if (error >= threshold)
		{
			error -= threshold;
			value += direction;
		}
		else
			error += minor;
	}
}

void abm_polygon(const ABitmap * bm, const ABitmapPoint * points, byte count, byte color)
{
	if (!count)
		return;

	const ABitmapPoint * previous = points + (count - 1);
	while (count--)
	{
		polygon_edge(bm, previous, points, color);
		previous = points++;
	}
}

#define BAND_ROWS 4

struct PolygonBand
{
	// The widest bound bitmap is the 384-pixel wide ANTIC F playfield.
	byte changes[BAND_ROWS * 48];
	byte top, bottom, first_byte, pitch;
	word width;
};

static __noinline void mark_edge(PolygonBand * band, const ABitmapPoint * a, const ABitmapPoint * b)
{
	const ABitmapPoint * upper = a->y < b->y ? a : b;
	const ABitmapPoint * lower = upper == a ? b : a;
	int start = upper->y > band->top ? upper->y : (int)band->top;
	int end = lower->y < band->bottom ? lower->y : (int)band->bottom;
	if (start >= end || (upper->x >= (int)band->width && lower->x >= (int)band->width))
		return;

	byte * row = band->changes + (word)(start - band->top) * band->pitch;
	int left = upper->x;
	int right = lower->x;
	if (left == right || (left <= 0 && right <= 0))
	{
		word column = left > 0 ? left : 0;
		byte bit = 1 << (column & 7);
		byte index = (column >> 3) - band->first_byte;
		for (int y = start; y < end; y++)
		{
			row[index] ^= bit;
			row += band->pitch;
		}
		return;
	}

	word dy = (word)lower->y - (word)upper->y;
	word dx = distance(upper->x, lower->x);
	bool leftward = lower->x < upper->x;
	word increment = dx < dy ? 0 : dx / dy;
	word remainder = dx - increment * dy;
	word threshold = dy - remainder;

	// A positive slope starts with dy - 1 to produce ceil(x). A negative
	// slope subtracts floor(run / dy). Seed a clipped edge once per band.
	word error = leftward ? 0 : dy - 1;
	word x = upper->x;
	word skip = (word)start - (word)upper->y;
	if (skip && dx)
	{
		word offset;
		if (skip <= 255 && dx <= 255 && dy <= 255)
		{
			word run = (word)(byte)skip * (byte)dx + error;
			offset = run / (byte)dy;
			error = run - offset * (byte)dy;
		}
		else
		{
			unsigned long run = (unsigned long)skip * dx + error;
			offset = run / dy;
			error = run - (unsigned long)offset * dy;
		}
		x = leftward ? x - offset : x + offset;
	}

	word unit = leftward ? 0xffffu : 1;
	if (leftward)
		increment = 0u - increment;
	for (int y = start; y < end; y++)
	{
		if ((int)x < 0)
			row[0] ^= 1;
		else if (x < band->width)
			row[(x >> 3) - band->first_byte] ^= 1 << (x & 7);

		x += increment;
		// Compare first so that the unsigned remainder cannot wrap.
		if (error >= threshold)
		{
			error -= threshold;
			x += unit;
		}
		else
			error += remainder;
		row += band->pitch;
	}
}

// Specialize the painters so ordinary bitmap fills keep their direct span
// writes and do not link the canvas renderer.
#define POLY_SCAN polygon_scan(const ABitmap * bm, const byte * changes, const PolygonBand * band, byte y, byte color)
#define POLY_SPAN(left, right) abm_fill_rect(bm, left, y, (right) - (left), 1, color)
#define POLY_WIDTH bm->width
#define POLY_FILL fill_bands(const ABitmap * bm, const ABitmapPoint * points, byte count, byte color)
#define POLY_BITMAP
#define POLY_CLIP
#define POLY_ROW polygon_scan(bm, row, &band, top, color)
#include "bitmap_polygon_raster.h"

static void paint_canvas(const ABitmapCanvas * canvas, word left, word right, byte y)
{
	ABitmapRect rect = {left, y, right - left, 1};
	abm_canvas_fill(canvas, &rect);
}

#define POLY_SCAN canvas_scan(const ABitmapCanvas * canvas, const byte * changes, const PolygonBand * band, byte y)
#define POLY_SPAN(left, right) paint_canvas(canvas, left, right, y)
#define POLY_WIDTH canvas->bitmap->width
#define POLY_FILL fill_canvas_bands(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count)
#define POLY_BITMAP const ABitmap * bm = canvas->bitmap;
#define POLY_CLIP \
	if (top < canvas->top) top = canvas->top; \
	if (bottom > canvas->bottom) bottom = canvas->bottom;
#define POLY_ROW canvas_scan(canvas, row, &band, top)
#include "bitmap_polygon_raster.h"

void abm_fill_polygon(const ABitmap * bm, const ABitmapPoint * points, byte count, byte color)
{
	if (count >= 3)
		fill_bands(bm, points, count, color);
	abm_polygon(bm, points, count, color);
}

void abm_canvas_polygon(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count)
{
	if (!count)
		return;
	if (count <= 2)
	{
		const ABitmapPoint * end = points + (count - 1);
		abm_canvas_line(canvas, points->x, points->y, end->x, end->y);
		return;
	}
	const ABitmapPoint * previous = points + (count - 1);
	while (count--)
	{
		abm_canvas_line(canvas, previous->x, previous->y, points->x, points->y);
		previous = points++;
	}
}

void abm_canvas_fill_polygon(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count)
{
	if (count >= 3 && canvas->left < canvas->right && canvas->top < canvas->bottom)
		fill_canvas_bands(canvas, points, count);
	abm_canvas_polygon(canvas, points, count);
}
