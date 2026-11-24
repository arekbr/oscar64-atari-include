#include <atari/colors.h>
#include <atari/input.h>
#include <fixmath.h>
#include <gfx/vector3d.h>
#include <stdlib.h>
#include "../support/bitmap_demo.h"

static F12Vector3 corners[8];
static ABitmapPoint points[8], previous[8];
static F12Matrix3 rmat, tmat;

// Toggle the points of a line in the packed one-bit screen.
static void xor_line(ABitmapPoint a, ABitmapPoint b)
{
	int dx = abs(b.x - a.x), dy = -abs(b.y - a.y);
	int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
	int error = dx + dy;
	for (;;)
	{
		screen.data[40 * a.y + (a.x >> 3)] ^= 0x80 >> (a.x & 7);
		if (a.x == b.x && a.y == b.y)
			break;
		int twice = 2 * error;
		if (twice >= dy)
		{
			error += dy;
			a.x += sx;
		}
		if (twice <= dx)
		{
			error += dx;
			a.y += sy;
		}
	}
}

static void cube(const ABitmapPoint * vertices)
{
	for (byte i = 0; i < 8; ++i)
		for (byte edge = 1; edge <= 4; edge <<= 1)
			if (!(i & edge))
				xor_line(vertices[i], vertices[i | edge]);
}

static __noinline void project_cube(byte k)
{
	f12mat3_set_rotate_x(&rmat, 0.1 * k);
	f12mat3_set_rotate_y(&tmat, 0.06 * k);
	f12mat3_mmul(&rmat, &tmat);
	for (byte i = 0; i < 8; ++i)
	{
		F12Vector3 vd;
		f12vec3_mmul(&vd, &rmat, corners + i);
		points[i].x = lmuldiv16s(vd.v[0], 140, vd.v[2] + 4 * FIX12_ONE) + 160;
		points[i].y = lmuldiv16s(vd.v[1], 140, vd.v[2] + 4 * FIX12_ONE) + 100;
	}

}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEF, 200))
		return 1;
	os_color[1] = COLOR_BLACK;
	os_color[2] = COLOR_WHITE;
	os_color[4] = COLOR_WHITE;

	for (byte i = 0; i < 8; ++i)
	{
		corners[i].v[0] = (i & 1) ? -FIX12_ONE : FIX12_ONE;
		corners[i].v[1] = (i & 2) ? -FIX12_ONE : FIX12_ONE;
		corners[i].v[2] = (i & 4) ? -FIX12_ONE : FIX12_ONE;
	}

	for (byte k = 0; k < 100; ++k)
	{
		project_cube(k);

		if (k)
			cube(previous);
		cube(points);
		for (byte i = 0; i < 8; ++i)
			previous[i] = points[i];
		if (cons_keys() & CONSOL_START)
			break;
	}

	bitmap_demo_restore(&saved);
	return 0;
}
