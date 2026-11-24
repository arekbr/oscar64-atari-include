#include "bitmap_canvas.h"
#include "bitmap_ellipse.h"

#include "bitmap_angle.h"

struct ArcShape
{
	const ABitmapCanvas * canvas;
	int x, y;
	long start_x, start_y, end_x, end_y;
	bool full, reflex, filled;
};

static void paint(const ArcShape * shape, int left, int right, int y)
{
	if (shape->filled)
	{
		ABitmapRect rect = {left, y, right - left, 1};
		abm_canvas_fill(shape->canvas, &rect);
	}
	else
		abm_fill_rect(shape->canvas->bitmap, left, y, right - left, 1, shape->canvas->pen_color);
}

static void arc_span(const ArcShape * shape, int left, int right, int y)
{
	const ABitmapCanvas * canvas = shape->canvas;
	y += shape->y;
	if (y < (int)canvas->top || y >= (int)canvas->bottom)
		return;
	left += shape->x;
	right += shape->x + 1;
	if (left < (int)canvas->left)
		left = canvas->left;
	if (right > (int)canvas->right)
		right = canvas->right;
	if (left >= right)
		return;
	if (shape->full)
	{
		paint(shape, left, right, y);
		return;
	}

	// Each cross product changes by a constant when stepping along the row.
	// A reflex sector takes the union of the two half planes.
	int dx = left - shape->x, dy = y - shape->y;
	long start = shape->start_x * dy - shape->start_y * dx;
	long end = shape->end_y * dx - shape->end_x * dy;
	int run = left;
	for (int x = left; x < right; x++)
	{
		bool a = start >= 0, b = end >= 0;
		if (!(shape->reflex ? a || b : a && b))
		{
			if (x > run)
				paint(shape, run, x, y);
			run = x + 1;
		}
		start -= shape->start_y;
		end += shape->end_y;
	}
	if (run < right)
		paint(shape, run, right, y);
}

static void arc_spans(const ArcShape * shape, int left, int right, byte row)
{
	arc_span(shape, left, right, row);
	if (row)
		arc_span(shape, left, right, -(int)row);
}

static void draw_arc(const ABitmapCanvas * canvas, const ABitmapArc * arc, bool filled)
{
	word rx = arc->rx;
	byte ry = arc->ry;
	word sweep = arc->sweep;
	if (!rx || !ry || rx > canvas->bitmap->width || ry > canvas->bitmap->height || !sweep || sweep > 256)
		return;
	if (arc->x < (int)canvas->left - (int)rx || arc->x >= (int)canvas->right + (int)rx ||
		arc->y < (int)canvas->top - ry || arc->y >= (int)canvas->bottom + ry)
		return;

	ArcShape shape;
	shape.canvas = canvas;
	shape.x = arc->x;
	shape.y = arc->y;
	shape.full = sweep == 256;
	shape.reflex = sweep > 128;
	shape.filled = filled;
	if (!shape.full)
	{
		byte end = arc->start + sweep;
		shape.start_x = (long)bitmap_sine(arc->start + 64) * rx;
		shape.start_y = (long)bitmap_sine(arc->start) * ry;
		shape.end_x = (long)bitmap_sine(end + 64) * rx;
		shape.end_y = (long)bitmap_sine(end) * ry;
	}

	long xx = (long)rx * rx, yy = (long)ry * ry;
	long dx = ((long)rx * 2 - 1) * yy;
	long dy = xx;
	long error = 0;
	int x = rx;
	for (byte row = 0; row < ry; row++)
	{
		int outer = x;
		ABM_ELLIPSE_NEXT(x, error, dx, dy, xx, yy);
		if (filled || !outer)
			arc_spans(&shape, -outer, outer, row);
		else
		{
			int edge = x < outer ? x + 1 : outer;
			arc_spans(&shape, -outer, -edge, row);
			arc_spans(&shape, edge, outer, row);
		}
	}
	arc_spans(&shape, -x, x, ry);
}

void abm_canvas_arc(const ABitmapCanvas * canvas, const ABitmapArc * arc)
{
	draw_arc(canvas, arc, false);
}

void abm_canvas_sector(const ABitmapCanvas * canvas, const ABitmapArc * arc)
{
	draw_arc(canvas, arc, true);
}
