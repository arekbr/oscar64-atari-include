#include "vbxe_canvas.h"

struct Flood
{
	const AVbxeCanvas * canvas;
	byte * marks, * pixels;
	word width, first, bytes;
	byte pitch, height, bias, before, after;
	bool packed;
};

static word row_bytes(const AVbxeCanvas * canvas)
{
	if (canvas->display->mode != VBXE_DISPLAY_HIGH)
		return canvas->right - canvas->left;
	return ((canvas->right + 1) >> 1) - (canvas->left >> 1);
}

word vbxe_canvas_flood_size(const AVbxeCanvas * canvas)
{
	if (!canvas || !canvas->display || canvas->left >= canvas->right || canvas->top >= canvas->bottom)
		return 0;
	word pitch = (canvas->right - canvas->left + 7) >> 3;
	return pitch * (canvas->bottom - canvas->top) + row_bytes(canvas);
}

#pragma optimize(push, 2, noinline, noconstparams)
static unsigned long row_address(const Flood * f, byte y)
{
	const AVbxeDisplay * d = f->canvas->display;
	return d->pixels + (unsigned long)(f->canvas->top + y) * d->pitch + f->first;
}

static __forceinline byte pixel(const Flood * f, word x)
{
	if (!f->packed)
		return f->pixels[x];
	x += f->bias;
	byte pair = f->pixels[x >> 1];
	return x & 1 ? pair & 15 : pair >> 4;
}

static word fill_run(const Flood * f, byte * visited, word x)
{
	word left = x, right = x + 1;
	while (left && pixel(f, left - 1) == f->before)
		left--;
	while (right < f->width && pixel(f, right) == f->before)
		right++;
	for (word pos = left; pos < right; pos++)
	{
		visited[pos >> 3] |= 1 << (pos & 7);
		if (f->packed)
		{
			word at = pos + f->bias;
			byte * cell = f->pixels + (at >> 1);
			*cell = at & 1 ? (*cell & 0xf0) | f->after : (*cell & 0x0f) | (f->after << 4);
		}
		else
			f->pixels[pos] = f->after;
	}
	return right;
}

// A read loads a whole clipped row once. Writes include the untouched
// nibble at an odd boundary and stay within the row's actual pixel bytes.
#pragma optimize(push, 0, noinline, noconstparams)
static __noinline sbyte flood_row(const Flood * f, byte y, const byte * previous, word seed)
{
	// The VBXE read can reuse the incoming pointer's argument slot.
	const byte * volatile source = previous;
	const AVbxe * board = f->canvas->display->board;
	unsigned long address = row_address(f, y);
	if (!vbxe_read(board, address, f->pixels, f->bytes))
		return -1;
	byte * visited = f->marks + (word)y * f->pitch;
	bool changed = false;
	if (!source)
	{
		fill_run(f, visited, seed);
		changed = true;
	}
	else
	{
		word x = 0;
		while (x < f->width)
		{
			byte ready = source[x >> 3] & ~visited[x >> 3];
			if (!ready)
				x = (x | 7) + 1;
			else if ((ready & (1 << (x & 7))) && pixel(f, x) == f->before)
			{
				x = fill_run(f, visited, x);
				changed = true;
			}
			else
				x++;
		}
	}
	if (!changed)
		return 0;
	return vbxe_write(board, address, f->pixels, f->bytes) ? 1 : -1;
}

#pragma optimize(pop)

static bool flood_rows(const Flood * f, word x, byte y)
{
	byte pending[30] = {0};
	if (flood_row(f, y, 0, x) < 0)
		return false;
	pending[y >> 3] = 1 << (y & 7);
	bool active;
	do
	{
		active = false;
		for (byte row = 0; row < f->height; row++)
		{
			byte mask = 1 << (row & 7);
			if (!(pending[row >> 3] & mask))
				continue;
			pending[row >> 3] &= ~mask;
			active = true;
			const byte * source = f->marks + (word)row * f->pitch;
			if (row)
			{
				byte above = row - 1;
				sbyte result = flood_row(f, above, source, 0);
				if (result < 0)
					return false;
				if (result)
					pending[above >> 3] |= 1 << (above & 7);
			}
			if (row + 1 < f->height)
			{
				byte below = row + 1;
				sbyte result = flood_row(f, below, source, 0);
				if (result < 0)
					return false;
				if (result)
					pending[below >> 3] |= 1 << (below & 7);
			}
		}
	} while (active);
	return true;
}

bool vbxe_canvas_flood_fill(const AVbxeCanvas * canvas, const ABitmapPoint * seed, byte color, byte * work, word size)
{
	if (!canvas || !canvas->display || !seed || seed->x < (int)canvas->left || seed->x >= (int)canvas->right ||
		seed->y < canvas->top || seed->y >= canvas->bottom)
		return false;
	bool packed = canvas->display->mode == VBXE_DISPLAY_HIGH;
	if (packed && color >= 16)
		return false;
	byte before;
	if (!vbxe_display_get(canvas->display, seed->x, seed->y, &before))
		return false;
	if (before == color)
		return true;

	word needed = vbxe_canvas_flood_size(canvas);
	word start = (word)work;
	if (!work || size < needed || start > 0xffffu - (needed - 1))
		return false;
	word end = start + needed - 1;
	if (start < VBXE_MEMAC_WINDOW + VBXE_MEMAC_SIZE && end >= VBXE_MEMAC_WINDOW)
		return false;

	word width = canvas->right - canvas->left;
	byte pitch = (width + 7) >> 3;
	byte height = canvas->bottom - canvas->top;
	word marks = (word)pitch * height;
	for (word i = 0; i < marks; i++)
		work[i] = 0;
	Flood f = {canvas, work, work + marks, width,
		packed ? canvas->left >> 1 : canvas->left, row_bytes(canvas),
		pitch, height, (byte)(packed && (canvas->left & 1)), before, color, packed};
	return flood_rows(&f, seed->x - canvas->left, seed->y - canvas->top);
}
#pragma optimize(pop)
