#include "vbxe_canvas.h"
#include "bitmap_angle.h"
#include "bitmap_ellipse.h"

#define VBXE_CANVAS_MAX_WIDTH 672

static int coord(long value, int limit)
{
	if (value < 0)
		return 0;
	return value > limit ? limit : (int)value;
}

static int signed_coord(long value)
{
	if (value < -32768l)
		return -32768;
	return value > 32767l ? 32767 : (int)value;
}

bool vbxe_canvas_init(AVbxeCanvas * canvas, const AVbxeDisplay * display)
{
	word width = vbxe_display_width(display);
	if (!canvas || !width || display->mode > VBXE_DISPLAY_HIGH || !display->board ||
		!display->height || display->height > 240 || display->pixels >= VBXE_VRAM_SIZE)
		return false;
	word bytes = display->mode == VBXE_DISPLAY_HIGH ? width >> 1 : width;
	unsigned long size = (unsigned long)(display->height - 1) * display->pitch + bytes;
	if (display->pitch < bytes || display->pitch > 4095 || size > VBXE_VRAM_SIZE - display->pixels)
		return false;

	canvas->display = display;
	canvas->width = width;
	vbxe_canvas_clip(canvas, 0);
	return true;
}

void vbxe_canvas_clip(AVbxeCanvas * canvas, const ABitmapRect * rect)
{
	word width = canvas->width;
	byte height = canvas->display->height;
	canvas->left = rect ? coord(rect->x, width) : 0;
	canvas->right = rect ? coord((long)rect->x + rect->width, width) : width;
	canvas->top = rect ? coord(rect->y, height) : 0;
	canvas->bottom = rect ? coord((long)rect->y + rect->height, height) : height;
}

static bool valid_color(const AVbxeCanvas * canvas, byte color)
{
	return canvas && canvas->display && (canvas->display->mode != VBXE_DISPLAY_HIGH || color < 16);
}

static bool empty(const AVbxeCanvas * canvas)
{
	return canvas->left >= canvas->right || canvas->top >= canvas->bottom;
}

#pragma optimize(push, 2, noinline, noconstparams)
static __noinline bool span(const AVbxeCanvas * canvas, int start, int stop, byte y, byte color)
{
	int left = (int)canvas->left, right = (int)canvas->right;
	if (start < left)
		start = left;
	if (stop > right)
		stop = right;
	return start >= stop || vbxe_display_rect(canvas->display, start, y, stop - 1, y, color);
}

static word distance(int a, int b)
{
	return a <= b ? (word)b - (word)a : (word)a - (word)b;
}

static __noinline bool axis_line(const AVbxeCanvas * canvas, const ABitmapPoint * from,
	const ABitmapPoint * to, byte color)
{
	int left = from->x < to->x ? from->x : to->x;
	int right = from->x > to->x ? from->x : to->x;
	int top = from->y < to->y ? from->y : to->y;
	int bottom = from->y > to->y ? from->y : to->y;
	if (left < (int)canvas->left)
		left = canvas->left;
	if (right >= (int)canvas->right)
		right = canvas->right - 1;
	if (top < (int)canvas->top)
		top = canvas->top;
	if (bottom >= (int)canvas->bottom)
		bottom = canvas->bottom - 1;
	return left > right || top > bottom || vbxe_display_rect(canvas->display, left, top, right, bottom, color);
}

__noinline bool vbxe_canvas_line(const AVbxeCanvas * canvas, const ABitmapPoint * from,
	const ABitmapPoint * to, byte color)
{
	if (!valid_color(canvas, color) || !from || !to)
		return false;
	if (empty(canvas))
		return true;
	if (from->x == to->x || from->y == to->y)
		return axis_line(canvas, from, to, color);

	word dx = distance(from->x, to->x), dy = distance(from->y, to->y);
	bool steep = dy > dx;
	word major = steep ? dy : dx, minor = steep ? dx : dy;
	int a = steep ? from->y : from->x, b = steep ? from->x : from->y;
	int a_end = steep ? to->y : to->x, b_end = steep ? to->x : to->y;
	int lo = steep ? canvas->top : (int)canvas->left;
	int hi = (steep ? canvas->bottom : (int)canvas->right) - 1;
	int b_lo = steep ? (int)canvas->left : canvas->top;
	int b_hi = steep ? (int)canvas->right : canvas->bottom;
	if ((b < b_lo && b_end < b_lo) || (b >= b_hi && b_end >= b_hi))
		return true;

	int step_a = a < a_end ? 1 : -1;
	int step_b = b < b_end ? 1 : -1;
	long first = step_a > 0 ? (long)lo - a : (long)a - hi;
	long last = step_a > 0 ? (long)hi - a : (long)a - lo;
	if (first > major || last < 0)
		return true;
	word i = first > 0 ? (word)first : 0;
	word end = last < major ? (word)last : major;

	// Seed Bresenham at the first visible major-axis coordinate.
	unsigned long advance = (unsigned long)i * minor + (major >> 1);
	word offset = advance / major;
	word error = advance - (unsigned long)offset * major;
	word threshold = major - minor;
	a = (int)(step_a > 0 ? (word)a + i : (word)a - i);
	b = (int)(step_b > 0 ? (word)b + offset : (word)b - offset);
	for (;;)
	{
		if (b >= b_lo && b < b_hi &&
			!vbxe_display_put(canvas->display, steep ? b : a, steep ? a : b, color))
			return false;
		if (i == end)
			return true;
		i++;
		a += step_a;
		if (error >= threshold)
		{
			error -= threshold;
			b += step_b;
		}
		else
			error += minor;
	}
}

bool vbxe_canvas_rect(const AVbxeCanvas * canvas, const ABitmapRect * rect, byte color, bool filled)
{
	if (!valid_color(canvas, color) || !rect)
		return false;
	if (empty(canvas) || !rect->width || !rect->height)
		return true;
	int right = signed_coord((long)rect->x + rect->width - 1);
	int bottom = signed_coord((long)rect->y + rect->height - 1);
	ABitmapPoint corners[4] = {{rect->x, rect->y}, {right, rect->y}, {right, bottom}, {rect->x, bottom}};
	if (filled)
		return axis_line(canvas, corners, corners + 2, color);
	return vbxe_canvas_polygon(canvas, corners, 4, color, false);
}

static void polygon_crossings(const AVbxeCanvas * canvas, const ABitmapPoint * points, byte count,
	byte y, byte * changes)
{
	const ABitmapPoint * previous = points + (count - 1);
	while (count--)
	{
		const ABitmapPoint * a = previous, * b = points;
		if (a->y > b->y)
		{
			a = points;
			b = previous;
		}
		previous = points++;
		if (a->y > y || b->y <= y)
			continue;

		word dy = (word)b->y - (word)a->y;
		word skip = (word)y - (word)a->y;
		unsigned long run = (unsigned long)skip * distance(a->x, b->x);
		long x = a->x;
		if (a->x <= b->x)
			x += (run + dy - 1) / dy;
		else
			x -= run / dy;
		if (x < (int)canvas->right)
		{
			word cell = x <= (int)canvas->left ? 0 : (word)x - canvas->left;
			changes[cell >> 3] ^= 1 << (cell & 7);
		}
	}
}

static __noinline bool fill_polygon(const AVbxeCanvas * canvas, const ABitmapPoint * points, byte count, byte color)
{
	int top = (int)canvas->bottom, bottom = (int)canvas->top;
	for (byte i = 0; i < count; i++)
	{
		if (points[i].y < top)
			top = points[i].y;
		if (points[i].y > bottom)
			bottom = points[i].y;
	}
	if (top < canvas->top)
		top = canvas->top;
	if (bottom > canvas->bottom)
		bottom = canvas->bottom;
	word width = canvas->right - canvas->left;
	byte bytes = (width + 7) >> 3;
	for (int y = top; y < bottom; y++)
	{
		byte changes[VBXE_CANVAS_MAX_WIDTH / 8];
		for (byte i = 0; i < bytes; i++)
			changes[i] = 0;
		polygon_crossings(canvas, points, count, y, changes);
		bool inside = false;
		word start = 0;
		for (byte i = 0; i < bytes; i++)
		{
			byte flags = changes[i];
			word x = canvas->left + ((word)i << 3);
			while (flags)
			{
				if (flags & 1)
				{
					if (inside && !span(canvas, start, x, y, color))
						return false;
					start = x;
					inside = !inside;
				}
				x++;
				flags >>= 1;
			}
		}
		if (inside && !span(canvas, start, canvas->right, y, color))
			return false;
	}
	return true;
}

bool vbxe_canvas_polygon(const AVbxeCanvas * canvas, const ABitmapPoint * points, byte count, byte color, bool filled)
{
	if (!valid_color(canvas, color) || (count && !points))
		return false;
	if (!count || empty(canvas))
		return true;
	if (count <= 2)
		return vbxe_canvas_line(canvas, points, points + (count - 1), color);
	if (filled && !fill_polygon(canvas, points, count, color))
		return false;
	const ABitmapPoint * previous = points + (count - 1);
	while (count--)
	{
		if (!vbxe_canvas_line(canvas, previous, points, color))
			return false;
		previous = points++;
	}
	return true;
}

struct ArcRaster
{
	const AVbxeCanvas * canvas;
	int x, y;
	long start_x, start_y, end_x, end_y;
	byte color;
	bool full, reflex;
};

static __noinline bool arc_row(const ArcRaster * arc, int left, int right, int y)
{
	const AVbxeCanvas * canvas = arc->canvas;
	y += arc->y;
	if (y < canvas->top || y >= canvas->bottom)
		return true;
	left += arc->x;
	right += arc->x + 1;
	if (left < (int)canvas->left)
		left = canvas->left;
	if (right > (int)canvas->right)
		right = canvas->right;
	if (left >= right)
		return true;
	if (arc->full)
		return span(canvas, left, right, y, arc->color);

	int dx = left - arc->x, dy = y - arc->y;
	long start = arc->start_x * dy - arc->start_y * dx;
	long end = arc->end_y * dx - arc->end_x * dy;
	int run = left;
	for (int x = left; x < right; x++)
	{
		bool a = start >= 0, b = end >= 0;
		if (!(arc->reflex ? a || b : a && b))
		{
			if (x > run && !span(canvas, run, x, y, arc->color))
				return false;
			run = x + 1;
		}
		start -= arc->start_y;
		end += arc->end_y;
	}
	return span(canvas, run, right, y, arc->color);
}

static bool arc_rows(const ArcRaster * arc, int left, int right, byte row)
{
	return arc_row(arc, left, right, row) && (!row || arc_row(arc, left, right, -(int)row));
}

bool vbxe_canvas_arc(const AVbxeCanvas * canvas, const ABitmapArc * arc, byte color, bool filled)
{
	if (!valid_color(canvas, color) || !arc || arc->sweep > 256 || arc->rx > canvas->width ||
		arc->ry > canvas->display->height)
		return false;
	if (!arc->sweep)
		return true;
	word rx = arc->rx;
	byte ry = arc->ry;
	if ((!rx || !ry) && arc->sweep != 256)
		return false;
	if (empty(canvas))
		return true;
	if (arc->x < (int)canvas->left - (int)rx || arc->x >= (int)canvas->right + (int)rx ||
		arc->y < (int)canvas->top - ry || arc->y >= (int)canvas->bottom + ry)
		return true;
	if (!rx || !ry)
	{
		ABitmapPoint a = {arc->x - (int)rx, arc->y - ry}, b = {arc->x + (int)rx, arc->y + ry};
		return axis_line(canvas, &a, &b, color);
	}

	ArcRaster raster;
	raster.canvas = canvas;
	raster.color = color;
	raster.x = arc->x;
	raster.y = arc->y;
	raster.full = arc->sweep == 256;
	raster.reflex = arc->sweep > 128;
	if (!raster.full)
	{
		byte end = arc->start + arc->sweep;
		raster.start_x = (long)bitmap_sine(arc->start + 64) * rx;
		raster.start_y = (long)bitmap_sine(arc->start) * ry;
		raster.end_x = (long)bitmap_sine(end + 64) * rx;
		raster.end_y = (long)bitmap_sine(end) * ry;
	}

	long xx = (long)rx * rx, yy = (long)ry * ry;
	long dx = (2l * rx - 1) * yy, dy = xx, error = 0;
	int x = rx;
	for (byte row = 0; row < ry; row++)
	{
		int outer = x;
		ABM_ELLIPSE_NEXT(x, error, dx, dy, xx, yy);
		if (filled || !outer)
		{
			if (!arc_rows(&raster, -outer, outer, row))
				return false;
		}
		else
		{
			int edge = x < outer ? x + 1 : outer;
			if (!arc_rows(&raster, -outer, -edge, row) || !arc_rows(&raster, edge, outer, row))
				return false;
		}
	}
	return arc_rows(&raster, -x, x, ry);
}

bool vbxe_canvas_bar_3d(const AVbxeCanvas * canvas, const ABitmapRect * front, word depth,
	byte fill, byte outline, bool top)
{
	if (!valid_color(canvas, fill) || !valid_color(canvas, outline) || !front)
		return false;
	if (!front->width || !front->height || empty(canvas))
		return true;
	if (!vbxe_canvas_rect(canvas, front, fill, true) || !vbxe_canvas_rect(canvas, front, outline, false))
		return false;
	if (!depth)
		return true;
	long right = (long)front->x + front->width - 1;
	long bottom = (long)front->y + front->height - 1;
	int far_x = signed_coord(right + depth), far_y = signed_coord((long)front->y - depth);
	ABitmapPoint side[4] = {{signed_coord(right), front->y}, {far_x, far_y},
		{far_x, signed_coord(bottom - depth)}, {signed_coord(right), signed_coord(bottom)}};
	if (!vbxe_canvas_polygon(canvas, side, 4, outline, false))
		return false;
	if (!top)
		return true;
	ABitmapPoint lid[4] = {{front->x, front->y}, {signed_coord((long)front->x + depth), far_y}, side[1], side[0]};
	return vbxe_canvas_polygon(canvas, lid, 4, outline, false);
}
#pragma optimize(pop)
