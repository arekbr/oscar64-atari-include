#ifndef ATARI_BITMAP_ELLIPSE_H
#define ATARI_BITMAP_ELLIPSE_H

// Advance one row of an integer ellipse, retaining the last in-bounds X.
// The arguments are local scalars and squared radii in each raster loop.
#define ABM_ELLIPSE_NEXT(x, error, dx, dy, xx, yy) do { \
	(error) += (dy); \
	(dy) += (xx) + (xx); \
	while ((error) > 0 && (x)) \
	{ \
		(error) -= (dx); \
		(dx) -= (yy) + (yy); \
		(x)--; \
	} \
} while (0)

#endif
