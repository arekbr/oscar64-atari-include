#ifndef ATARI_BITMAP_CURVES_H
#define ATARI_BITMAP_CURVES_H

#include "bitmap_canvas.h"

// Approximate each curve section by up to 128 clipped pen segments.
// A zero segment count returns false. Drawing preserves canvas position.
// The quadratic interpolates all three points, with points[1] at t = 1/2.
// An odd segment count rounds up to include the middle point.
bool abm_canvas_quadratic(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte segments);

// Points 0 and 3 are endpoints. Points 1 and 2 control the tangents.
bool abm_canvas_bezier(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte segments);

// Draw from points[1] through points[count - 2]. Duplicate the first and
// last endpoints to include them. Pass at least four points.
bool abm_canvas_catmull_rom(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count, byte segments);

// Natural cubic spline with chord-length knots, as in Mad Pascal's BSpline.
// It interpolates 2..11 points and coalesces adjacent duplicates.
#define ABM_SPLINE_MAX_POINTS 11
bool abm_canvas_spline(const ABitmapCanvas * canvas, const ABitmapPoint * points, byte count, byte segments);

#pragma compile("bitmap_curves.c")

#endif
