#ifndef ATARI_SAMPLE_SURFACE_H
#define ATARI_SAMPLE_SURFACE_H

#include <atari/bitmap_canvas.h>
#include <atari/colors.h>
#include <atari/input.h>
#include <gfx/vector3d.h>
#include <math.h>
#define ATARI_SAMPLE_CUSTOM_REGION
#include "bitmap_demo.h"
#include "stipple.h"

#pragma stacksize(1024)
#pragma heapsize(0)
#pragma region(main, 0x2080, 0x8010, , , {code, data, bss, heap, stack})

#define HALF 15
#define FULL (2 * HALF)
#define SIZE (FULL + 1)
#define FACES (FULL * FULL)

struct Face
{
	float depth;
	byte x, y;
};

static float height[SIZE][SIZE];
static ABitmapPoint points[SIZE][SIZE];
static ABitmapPoint quad[4];
static Matrix4 world, projection, transform, rotation;
static Vector3 light;
static ABitmapCanvas canvas;

// BASIC must be disabled. This workspace ends below the OS text screen.
#pragma section(faces, 0)
#pragma region(faces, 0xa000, 0xb800, , , {faces})
#pragma bss(faces)
static Face faces[FACES];
#pragma bss(bss)

static void sort_faces(Face * items, int count)
{
	if (count > 1)
	{
		Face pivot = items[0];
		int at = 0;
		for (int i = 1; i < count; ++i)
			if (items[i].depth > pivot.depth)
			{
				items[at] = items[i];
				items[i] = items[++at];
			}
		items[at] = pivot;
		sort_faces(items, at);
		sort_faces(items + at + 1, count - at - 1);
	}
}

static void vertex(Vector3 * v, byte x, byte y)
{
	vec3_set(v, (x - HALF) * (1.0 / HALF), height[y][x], (HALF - y) * (1.0 / HALF));
}

static void sample_function(void)
{
	for (byte x = 0; x < SIZE; ++x)
		for (byte y = 0; y < SIZE; ++y)
		{
			float rx = (x - HALF) * (1.0 / HALF), ry = (HALF - y) * (1.0 / HALF);
			float r = sqrt(rx * rx + ry * ry);
			height[y][x] = -cos(r * 16) * exp(-2 * r) * 0.5;
		}
}

static void project_vertices(bool multicolor)
{
	mat4_ident(&world);
	mat4_make_perspective(&projection, 0.5 * PI, 1.0, 0.0, 200.0);
	mat4_scale(&world, 18);
	mat4_set_rotate_x(&rotation, -0.98);
	mat4_set_rotate_y(&transform, 0.3);
	mat4_rmmul(&rotation, &transform);
	mat4_rmmul(&rotation, &world);
	rotation.m[14] += 20.0;
	transform = projection;
	mat4_mmul(&transform, &rotation);

	// Accumulate each face's four adjacent vertex depths in raster order.
	for (byte y = 0; y < SIZE; ++y)
		for (byte x = 0; x < SIZE; ++x)
		{
			Vector3 v, p;
			vertex(&v, x, y);
			vec3_project(&p, &transform, &v);
			int px = (int)(p.v[0] * 140 + 160);
			points[y][x].x = multicolor ? px >> 1 : px;
			points[y][x].y = (int)(p.v[1] * 140 + 80);
			if (y < FULL && x < FULL)
			{
				Face * face = &faces[(word)y * FULL + x];
				face->x = x;
				face->y = y;
				face->depth = p.v[2];
			}
			if (y < FULL && x > 0)
				faces[(word)y * FULL + x - 1].depth += p.v[2];
			if (y > 0 && x < FULL)
				faces[(word)(y - 1) * FULL + x].depth += p.v[2];
			if (y > 0 && x > 0)
				faces[(word)(y - 1) * FULL + x - 1].depth += p.v[2];
		}
}

static void fill_quad(byte low, byte high, const byte * pattern)
{
	byte complement[8];
	for (byte y = 0; y < 8; ++y)
		complement[y] = ~pattern[y];
	abm_canvas_brush(&canvas, low, pattern);
	abm_canvas_fill_polygon(&canvas, quad, 4);
	abm_canvas_brush(&canvas, high, complement);
	abm_canvas_fill_polygon(&canvas, quad, 4);
}

static void draw_face(const Face * face, bool multicolor)
{
	byte x = face->x, y = face->y;
	Vector3 a, b, c, d, d0, d1, n;
	vertex(&a, x, y);
	vertex(&b, x + 1, y);
	vertex(&c, x + 1, y + 1);
	vertex(&d, x, y + 1);
	vec3_diff(&d0, &a, &c);
	vec3_diff(&d1, &d, &b);
	vec3_xmul(&n, &d0, &d1);
	vec3_norm(&n);
	float f = vec3_vmul(&light, &n);
	if (f > 0.999)
		f = 0.999;

	quad[0] = points[y][x];
	quad[1] = points[y][x + 1];
	quad[2] = points[y + 1][x + 1];
	quad[3] = points[y + 1][x];
	abm_canvas_pen(&canvas, 0, 0, 0);
	if (multicolor)
	{
		byte shade = f > 0 ? 1 + (byte)(f * 6) : 0;
		fill_quad(shade >> 1, (shade + 1) >> 1, gray[4]);
	}
	else
	{
		byte shade = f > 0 ? 8 - (byte)(f * 9) : 8;
		fill_quad(1, 0, gray[shade]);
		abm_canvas_pen(&canvas, 1, shade < 5 ? 0xffff : 0xaaaa, 1);
		abm_canvas_polygon(&canvas, quad, 4);
		if (shade >= 5)
		{
			abm_canvas_pen(&canvas, 0, 0x5555, 1);
			abm_canvas_polygon(&canvas, quad, 4);
		}
	}
}

static void surface(bool multicolor)
{
	const byte * font = (const byte *)0xe000;
	abm_canvas_init(&canvas, &screen);
	if (multicolor)
	{
		os_color[0] = COLOR_GRAY2;
		os_color[1] = COLOR_GRAY3;
		os_color[2] = COLOR_WHITE;
		os_color[4] = COLOR_GRAY1;
	}
	else
	{
		os_color[1] = COLOR_BLACK;
		os_color[2] = COLOR_WHITE;
		os_color[4] = COLOR_WHITE;
	}
	byte letter = multicolor ? 3 : 1;
	abm_text(&screen, 0, 0, "Preparing function", font, letter);
	sample_function();
	abm_text(&screen, 0, 8, "Projecting vertices", font, letter);
	project_vertices(multicolor);
	abm_text(&screen, 0, 16, "Sorting surfaces", font, letter);
	sort_faces(faces, FACES);
	abm_text(&screen, 0, 24, "Drawing surfaces", font, letter);
	vec3_set(&light, 2.0, -2.0, -1.0);
	vec3_norm(&light);
	for (word i = 0; i < FACES; ++i)
		draw_face(&faces[i], multicolor);
}

#endif
