#include "bitmap_curves.h"
#include <math.h>

struct Polynomial
{
	float a, b, c, d;
};

struct Curve
{
	Polynomial x, y;
};

static int curve_coord(float value)
{
	if (value <= -32768.0f)
		return -32768;
	if (value >= 32767.0f)
		return 32767;
	return (int)(value + (value < 0.0f ? -0.5f : 0.5f));
}

static float evaluate(const Polynomial * p, float t)
{
	return ((p->a * t + p->b) * t + p->c) * t + p->d;
}

static void draw_curve(const ABitmapCanvas * canvas, const Curve * curve, byte segments)
{
	int x = curve_coord(curve->x.d), y = curve_coord(curve->y.d);
	for (byte i = 1; i <= segments; i++)
	{
		float t = (float)i / segments;
		int next_x = curve_coord(evaluate(&curve->x, t));
		int next_y = curve_coord(evaluate(&curve->y, t));
		abm_canvas_line(canvas, x, y, next_x, next_y);
		x = next_x;
		y = next_y;
	}
}

static byte segment_count(byte segments)
{
	return segments > 128 ? 128 : segments;
}

// A specialized -O3 call can convert a negative literal coordinate
// as unsigned when storing the float coefficients.
#pragma optimize(push, 2, noinline, noconstparams)
static __noinline void quadratic(Polynomial * p, int first, int middle, int last)
{
	p->a = 0;
	p->b = 2.0f * ((float)first - 2.0f * middle + last);
	p->c = (float)last - first - p->b;
	p->d = first;
}

#pragma optimize(pop)

bool abm_canvas_quadratic(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte segments)
{
	if (!canvas || !points || !segments)
		return false;

	Curve curve;
	quadratic(&curve.x, points[0].x, points[1].x, points[2].x);
	quadratic(&curve.y, points[0].y, points[1].y, points[2].y);
	segments = segment_count(segments);
	draw_curve(canvas, &curve, segments + (segments & 1));
	return true;
}

static void bezier(Polynomial * p, int first, int left, int right, int last)
{
	p->c = 3.0f * ((float)left - first);
	p->b = 3.0f * ((float)right - 2.0f * left + first);
	p->a = (float)last - first - p->c - p->b;
	p->d = first;
}

bool abm_canvas_bezier(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte segments)
{
	if (!canvas || !points || !segments)
		return false;

	Curve curve;
	bezier(&curve.x, points[0].x, points[1].x, points[2].x, points[3].x);
	bezier(&curve.y, points[0].y, points[1].y, points[2].y, points[3].y);
	draw_curve(canvas, &curve, segment_count(segments));
	return true;
}

static void catmull(Polynomial * p, int prev, int first, int last, int next)
{
	float start = 0.5f * ((float)last - prev);
	float end = 0.5f * ((float)next - first);
	float run = (float)last - first;
	p->a = start + end - 2.0f * run;
	p->b = 3.0f * run - 2.0f * start - end;
	p->c = start;
	p->d = first;
}

bool abm_canvas_catmull_rom(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count, byte segments)
{
	if (!canvas || !points || count < 4 || !segments)
		return false;
	segments = segment_count(segments);
	for (byte i = 3; i < count; i++, points++)
	{
		Curve curve;
		catmull(&curve.x, points[0].x, points[1].x, points[2].x, points[3].x);
		catmull(&curve.y, points[0].y, points[1].y, points[2].y, points[3].y);
		draw_curve(canvas, &curve, segments);
	}
	return true;
}

struct Knot
{
	ABitmapPoint point;
	float length, diagonal, mx, my;
};

// Eliminate the lower diagonal, then back-substitute the second
// derivatives. The two endpoint moments are zero.
static void spline_moments(Knot * knot, byte last)
{
	knot[0].mx = knot[0].my = knot[last].mx = knot[last].my = 0;
	for (byte i = 1; i < last; i++)
	{
		Knot * k = knot + i;
		float before = k[-1].length, after = k->length;
		k->diagonal = 2.0f * (before + after);
		k->mx = 6.0f * (((float)k[1].point.x - k->point.x) / after -
			((float)k->point.x - k[-1].point.x) / before);
		k->my = 6.0f * (((float)k[1].point.y - k->point.y) / after -
			((float)k->point.y - k[-1].point.y) / before);
		if (i > 1)
		{
			float scale = before / k[-1].diagonal;
			k->diagonal -= scale * before;
			k->mx -= scale * k[-1].mx;
			k->my -= scale * k[-1].my;
		}
	}
	while (--last)
	{
		Knot * k = knot + last;
		k->mx = (k->mx - k->length * k[1].mx) / k->diagonal;
		k->my = (k->my - k->length * k[1].my) / k->diagonal;
	}
}

static void spline(Polynomial * p, int a, int b, float left, float right, float scale)
{
	p->a = (right - left) * scale;
	p->b = 3.0f * left * scale;
	p->c = (float)b - a - p->a - p->b;
	p->d = a;
}

bool abm_canvas_spline(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count, byte segments)
{
	if (!canvas || !points || count < 2 || count > ABM_SPLINE_MAX_POINTS || !segments)
		return false;

	Knot knot[ABM_SPLINE_MAX_POINTS];
	knot[0].point = points[0];
	byte last = 0;
	for (byte i = 1; i < count; i++)
	{
		float x = (float)points[i].x - knot[last].point.x;
		float y = (float)points[i].y - knot[last].point.y;
		if (x != 0.0f || y != 0.0f)
		{
			knot[last].length = sqrt(x * x + y * y);
			knot[++last].point = points[i];
		}
	}
	if (!last)
	{
		abm_canvas_line(canvas, points[0].x, points[0].y, points[0].x, points[0].y);
		return true;
	}

	spline_moments(knot, last);
	segments = segment_count(segments);
	for (byte i = 0; i < last; i++)
	{
		const Knot * k = knot + i;
		float scale = k->length * k->length / 6.0f;
		Curve curve;
		spline(&curve.x, k->point.x, k[1].point.x, k->mx, k[1].mx, scale);
		spline(&curve.y, k->point.y, k[1].point.y, k->my, k[1].my, scale);
		draw_curve(canvas, &curve, segments);
	}
	return true;
}
