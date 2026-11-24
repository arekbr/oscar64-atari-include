#include "bitmap_canvas.h"
#include "charwin.h"

static word clip_coord(long value, word limit)
{
	if (value < 0)
		return 0;
	return value > limit ? limit : (word)value;
}

void abm_canvas_init(ABitmapCanvas * canvas, const ABitmap * bitmap)
{
	canvas->bitmap = bitmap;
	abm_canvas_clip(canvas, 0);
	abm_canvas_move(canvas, 0, 0);
	abm_canvas_pen(canvas, 1, 0xffffu, 1);
	abm_canvas_brush(canvas, 1, 0);
}

void abm_canvas_clip(ABitmapCanvas * canvas, const ABitmapRect * rect)
{
	word width = canvas->bitmap->width;
	byte height = canvas->bitmap->height;
	canvas->left = rect ? clip_coord(rect->x, width) : 0;
	canvas->top = rect ? clip_coord(rect->y, height) : 0;
	canvas->right = rect ? clip_coord((long)rect->x + rect->width, width) : width;
	canvas->bottom = rect ? clip_coord((long)rect->y + rect->height, height) : height;
}

void abm_canvas_pen(ABitmapCanvas * canvas, byte color, word pattern, byte width)
{
	canvas->pen_color = color;
	canvas->line_pattern = pattern;
	canvas->line_width = width;
}

void abm_canvas_brush(ABitmapCanvas * canvas, byte color, const byte * pattern)
{
	canvas->brush_color = color;
	canvas->brush = pattern;
}

void abm_canvas_move(ABitmapCanvas * canvas, int x, int y)
{
	canvas->x = x;
	canvas->y = y;
}

void abm_canvas_to(ABitmapCanvas * canvas, int x, int y)
{
	abm_canvas_line(canvas, canvas->x, canvas->y, x, y);
	abm_canvas_move(canvas, x, y);
}

static void pen_stamp(const ABitmapCanvas * canvas, int x, int y)
{
	int before = (canvas->line_width - 1) >> 1;
	int after = canvas->line_width - before;
	if (x < (int)canvas->left - after + 1 || x >= (int)canvas->right + before ||
		y < (int)canvas->top - after + 1 || y >= (int)canvas->bottom + before)
		return;
	int left = x - before, right = x + after, top = y - before, bottom = y + after;
	if (left < (int)canvas->left)
		left = canvas->left;
	if (right > (int)canvas->right)
		right = canvas->right;
	if (top < (int)canvas->top)
		top = canvas->top;
	if (bottom > (int)canvas->bottom)
		bottom = canvas->bottom;
	abm_fill_rect(canvas->bitmap, left, top, right - left, bottom - top, canvas->pen_color);
}

void abm_canvas_line(const ABitmapCanvas * canvas, int x0, int y0, int x1, int y1)
{
	if (!canvas->line_pattern || !canvas->line_width ||
		canvas->left == canvas->right || canvas->top == canvas->bottom)
		return;

	word dx = x0 <= x1 ? (word)x1 - (word)x0 : (word)x0 - (word)x1;
	word dy = y0 <= y1 ? (word)y1 - (word)y0 : (word)y0 - (word)y1;
	bool vertical = dy > dx;
	word major = vertical ? dy : dx;
	word minor = vertical ? dx : dy;
	if (!major)
	{
		if (canvas->line_pattern & 0x8000u)
			pen_stamp(canvas, x0, y0);
		return;
	}

	int a = vertical ? y0 : x0, b = vertical ? x0 : y0;
	int a_step = (vertical ? y0 <= y1 : x0 <= x1) ? 1 : -1;
	int b_step = (vertical ? x0 <= x1 : y0 <= y1) ? 1 : -1;
	int before = (canvas->line_width - 1) >> 1;
	int after = canvas->line_width - before;
	int lo = (vertical ? canvas->top : (int)canvas->left) - after + 1;
	int hi = (vertical ? canvas->bottom : (int)canvas->right) + before - 1;
	long first = a_step > 0 ? (long)lo - a : (long)a - hi;
	long last = a_step > 0 ? (long)hi - a : (long)a - lo;
	if (first > major || last < 0)
		return;
	word i = first > 0 ? (word)first : 0;
	word end = last < major ? (word)last : major;

	// Jump to the first visible major-axis step while retaining Bresenham
	// rounding and the pattern phase from the original start point.
	unsigned long offset = (unsigned long)i * minor + (major >> 1);
	word distance = offset / major;
	word error = offset % major;
	a = (int)(a_step > 0 ? (word)a + i : (word)a - i);
	b = (int)(b_step > 0 ? (word)b + distance : (word)b - distance);
	word mask = 0x8000u >> (i & 15);
	word threshold = major - minor;
	for (;;)
	{
		if (canvas->line_pattern & mask)
		{
			if (vertical)
				pen_stamp(canvas, b, a);
			else
				pen_stamp(canvas, a, b);
		}
		if (i == end)
			break;
		i++;
		a += a_step;
		if (error >= threshold)
		{
			error -= threshold;
			b += b_step;
		}
		else
			error += minor;
		mask = mask == 1 ? 0x8000u : mask >> 1;
	}
}

void abm_canvas_rect(const ABitmapCanvas * canvas, int x0, int y0, int x1, int y1)
{
	abm_canvas_line(canvas, x0, y0, x1, y0);
	abm_canvas_line(canvas, x1, y0, x1, y1);
	abm_canvas_line(canvas, x1, y1, x0, y1);
	abm_canvas_line(canvas, x0, y1, x0, y0);
}

void abm_canvas_fill(const ABitmapCanvas * canvas, const ABitmapRect * rect)
{
	int left = rect->x, top = rect->y;
	long right = (long)rect->x + rect->width;
	long bottom = (long)rect->y + rect->height;
	if (left < (int)canvas->left)
		left = canvas->left;
	if (top < (int)canvas->top)
		top = canvas->top;
	if (right > canvas->right)
		right = canvas->right;
	if (bottom > canvas->bottom)
		bottom = canvas->bottom;
	if (left >= right || top >= bottom)
		return;
	if (!canvas->brush)
	{
		abm_fill_rect(canvas->bitmap, left, top, (int)right - left, (int)bottom - top, canvas->brush_color);
		return;
	}
	for (int y = top; y < (int)bottom; y++)
	{
		byte bits = canvas->brush[y & 7];
		for (int x = left; x < (int)right; x++)
			if (bits & (0x80 >> (x & 7)))
				abm_put(canvas->bitmap, x, y, canvas->brush_color);
	}
}

static int saturate_coord(long value)
{
	if (value < -32768l)
		return -32768;
	if (value > 32767l)
		return 32767;
	return value;
}

void abm_canvas_bar_3d(const ABitmapCanvas * canvas, const ABitmapRect * front, word depth, bool top)
{
	if (!front->width || !front->height)
		return;
	int x0 = front->x, y0 = front->y;
	long right = (long)x0 + front->width - 1;
	long bottom = (long)y0 + front->height - 1;
	int x1 = saturate_coord(right), y1 = saturate_coord(bottom);
	abm_canvas_fill(canvas, front);
	abm_canvas_rect(canvas, x0, y0, x1, y1);
	if (!depth)
		return;
	int far_right = saturate_coord(right + depth);
	int far_top = saturate_coord((long)y0 - depth);
	int far_bottom = saturate_coord(bottom - depth);
	if (top)
	{
		int far_left = saturate_coord((long)x0 + depth);
		abm_canvas_line(canvas, x0, y0, far_left, far_top);
		abm_canvas_line(canvas, far_left, far_top, far_right, far_top);
		abm_canvas_line(canvas, far_right, far_top, x1, y0);
	}
	abm_canvas_line(canvas, far_right, far_top, far_right, far_bottom);
	abm_canvas_line(canvas, far_right, far_bottom, x1, y1);
}

static byte glyph_width(const ABitmapFont * font, byte index)
{
	byte width = font->widths ? font->widths[index] : 8;
	return width > 8 ? 8 : width;
}

word abm_canvas_text_width(const char * text, const ABitmapFont * font)
{
	word total = 0;
	while (*text)
	{
		word advance = glyph_width(font, cwin_a2s(*text++) & 0x7f);
		if (*text)
			advance += font->spacing;
		if (total > 0xffffu - advance)
			return 0xffffu;
		total += advance;
	}
	return total;
}

void abm_canvas_text(const ABitmapCanvas * canvas, const char * text, const ABitmapFont * font)
{
	int x = canvas->x, y = canvas->y;
	if (y < (int)canvas->top - 7 || y >= (int)canvas->bottom)
		return;
	while (*text && x < (int)canvas->right)
	{
		byte code = cwin_a2s(*text++);
		byte width = glyph_width(font, code & 0x7f);
		if (x >= (int)canvas->left - 7)
		{
			const byte * glyph = font->glyphs + ((word)(code & 0x7f) << 3);
			byte inverse = code & 0x80 ? 0xff : 0;
			for (byte row = 0; row < 8; row++)
			{
				int py = y + row;
				if (py >= (int)canvas->top && py < (int)canvas->bottom)
				{
					byte bits = glyph[row] ^ inverse;
					for (byte col = 0; col < width; col++)
					{
						int px = x + col;
						if ((bits & (0x80 >> col)) && px >= (int)canvas->left && px < (int)canvas->right)
							abm_put(canvas->bitmap, px, py, canvas->pen_color);
					}
				}
			}
		}
		x += (word)width + font->spacing;
	}
}
