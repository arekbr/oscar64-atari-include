#include "bitmap.h"
#include "dlist.h"

static const ABitmapMode modes[] = {
	{ 40, 10, 2, 8, DL_MODE8 },
	{ 80, 10, 1, 4, DL_MODE9 },
	{ 80, 20, 2, 4, DL_MODEA },
	{ 160, 20, 1, 2, DL_MODEB },
	{ 160, 20, 1, 1, DL_MODEC },
	{ 160, 40, 2, 2, DL_MODED },
	{ 160, 40, 2, 1, DL_MODEE },
	{ 320, 40, 1, 1, DL_MODEF },
	{ 80, 40, 4, 1, DL_MODEF }
};

const ABitmapMode * abm_mode_info(byte mode)
{
	if (mode >= DL_MODE8 && mode <= ABM_GTIA)
		return modes + (mode - DL_MODE8);
	return 0;
}

bool abm_mode_info_width(byte mode, byte playfield, ABitmapMode * info)
{
	const ABitmapMode * normal = abm_mode_info(mode);
	if (!normal || !info || playfield < DMACTL_PF_NARROW || playfield > DMACTL_PF_WIDE)
		return false;

	*info = *normal;
	if (playfield != DMACTL_PF_NORMAL)
	{
		byte units = playfield == DMACTL_PF_NARROW ? 4 : 6;
		info->row_bytes = (normal->row_bytes / 5) * units;
		info->width = (normal->width / 5) * units;
	}
	return true;
}

bool abm_init_width(ABitmap * bm, volatile byte * data, word size, byte pitch, byte height, byte mode, byte playfield)
{
	ABitmapMode info;
	if (!abm_mode_info_width(mode, playfield, &info) || !bm || !data || pitch < info.row_bytes || !height)
		return false;

	word needed = (word)(height - 1) * pitch + info.row_bytes;
	if (size < needed || (word)data > 0xffffu - (needed - 1))
		return false;

	bm->data = data;
	bm->width = info.width;
	bm->height = height;
	bm->pitch = pitch;
	bm->bits = info.bits;
	return true;
}

bool abm_init(ABitmap * bm, volatile byte * data, word size, byte pitch, byte height, byte mode)
{
	return abm_init_width(bm, data, size, pitch, height, mode, DMACTL_PF_NORMAL);
}

static word pixel_bit(const ABitmap * bm, word x)
{
	if (bm->bits == 1)
		return x;
	return bm->bits == 2 ? x << 1 : x << 2;
}

byte abm_get(const ABitmap * bm, word x, byte y)
{
	if (x >= bm->width || y >= bm->height)
		return 0;

	word bit = pixel_bit(bm, x);
	byte shift = 8 - bm->bits - (bit & 7);
	byte mask = (1 << bm->bits) - 1;
	word offset = (word)y * bm->pitch + (bit >> 3);
	return (bm->data[offset] >> shift) & mask;
}

volatile byte * abm_row(const ABitmap * bm, byte y)
{
	return y < bm->height ? bm->data + (word)y * bm->pitch : (volatile byte *)0;
}

static void put_pixel(const ABitmap * bm, word x, byte y, byte color)
{
	word bit = pixel_bit(bm, x);
	byte shift = 8 - bm->bits - (bit & 7);
	byte mask = (1 << bm->bits) - 1;
	word offset = (word)y * bm->pitch + (bit >> 3);
	volatile byte * pixel = bm->data + offset;
	*pixel = (*pixel & ~(mask << shift)) | ((color & mask) << shift);
}

void abm_put(const ABitmap * bm, word x, byte y, byte color)
{
	if (x < bm->width && y < bm->height)
		put_pixel(bm, x, y, color);
}

static byte fill_byte(byte bits, byte color)
{
	if (bits == 1)
		return (color & 1) ? 0xff : 0;
	if (bits == 2)
		return (color & 3) * 0x55;
	return (color & 15) * 0x11;
}

void abm_clear(const ABitmap * bm, byte color)
{
	byte value = fill_byte(bm->bits, color);
	byte row_bytes = pixel_bit(bm, bm->width) >> 3;
	for (byte y = 0; y < bm->height; y++)
	{
		volatile byte * row = bm->data + (word)y * bm->pitch;
		for (byte x = 0; x < row_bytes; x++)
			row[x] = value;
	}
}

void abm_line(const ABitmap * bm, word x0, byte y0, word x1, byte y1, byte color)
{
	if (x0 >= bm->width || x1 >= bm->width || y0 >= bm->height || y1 >= bm->height)
		return;

	int dx = (int)x1 - (int)x0;
	int dy = (int)y1 - (int)y0;
	int sx = 1, sy = 1;
	if (dx < 0)
	{
		dx = -dx;
		sx = -1;
	}
	if (dy < 0)
	{
		dy = -dy;
		sy = -1;
	}

	int error = dx - dy;
	for (;;)
	{
		put_pixel(bm, x0, y0, color);
		if (x0 == x1 && y0 == y1)
			return;

		int twice = error + error;
		if (twice >= -dy)
		{
			error -= dy;
			x0 += sx;
		}
		if (twice <= dx)
		{
			error += dx;
			y0 += sy;
		}
	}
}

static void fill_masked(volatile byte * cell, byte value, byte mask)
{
	*cell = (*cell & ~mask) | (value & mask);
}

void abm_fill_rect(const ABitmap * bm, word x, byte y, word width, byte height, byte color)
{
	if (!width || !height || x >= bm->width || y >= bm->height)
		return;
	if (width > bm->width - x)
		width = bm->width - x;
	if (height > bm->height - y)
		height = bm->height - y;

	word start = pixel_bit(bm, x);
	word end = pixel_bit(bm, x + width) - 1;
	byte first = start >> 3;
	byte last = end >> 3;
	byte left_mask = 0xff >> (start & 7);
	byte right_mask = 0xff << (7 - (end & 7));
	if (first == last)
		right_mask &= left_mask;
	byte value = fill_byte(bm->bits, color);

	for (byte i = 0; i < height; i++)
	{
		volatile byte * row = bm->data + (word)(y + i) * bm->pitch;
		if (first != last)
		{
			fill_masked(row + first, value, left_mask);
			for (byte col = first + 1; col < last; col++)
				row[col] = value;
		}
		fill_masked(row + last, value, right_mask);
	}
}

static bool needs_lms(byte row, word addr, byte pitch, byte row_bytes)
{
	return !row || pitch != row_bytes || (addr & 0x0fff) < pitch;
}

word abm_display_list_size(const volatile byte * list, word capacity, const ABitmap * bm, byte mode, byte top)
{
	const ABitmapMode * normal = abm_mode_info(mode);
	if (!normal || !list || !bm || !bm->height || bm->bits != normal->bits ||
	    (word)bm->height * normal->scanlines + top > 240)
		return 0;

	byte playfield = DMACTL_PF_NORMAL;
	if (bm->width != normal->width)
		playfield = bm->width < normal->width ? DMACTL_PF_NARROW : DMACTL_PF_WIDE;

	ABitmapMode info;
	abm_mode_info_width(mode, playfield, &info);
	if (bm->width != info.width || bm->pitch < info.row_bytes)
		return 0;

	word screen = (word)bm->data;
	word last_offset = (word)(bm->height - 1) * bm->pitch + info.row_bytes - 1;
	if (!screen || screen > 0xffffu - last_offset)
		return 0;

	word length = ((word)top + 7) / 8 + bm->height + 3;
	word addr = screen;
	for (byte row = 0; row < bm->height; row++)
	{
		if ((addr & 0x0fff) > 4096 - info.row_bytes)
			return 0;
		if (needs_lms(row, addr, bm->pitch, info.row_bytes))
			length += 2;
		addr += bm->pitch;
	}

	word start = (word)list;
	if (length > capacity || length > 1024 - (start & 0x03ff))
		return 0;
	if (start <= screen + last_offset && start + (length - 1) >= screen)
		return 0;
	return length;
}


word abm_display_list(volatile byte * list, word capacity, const ABitmap * bm, byte mode, byte top)
{
	word length = abm_display_list_size(list, capacity, bm, mode, top);
	if (!length)
		return 0;
	const ABitmapMode * info = abm_mode_info(mode);
	byte row_bytes = (bm->width * bm->bits) >> 3;
	volatile byte * dl = dl_blank(list, top);
	word addr = (word)bm->data;
	for (byte row = 0; row < bm->height; row++)
	{
		if (needs_lms(row, addr, bm->pitch, row_bytes))
			dl = dl_lms(dl, info->antic_mode, (const volatile void *)addr);
		else
			*dl++ = info->antic_mode;
		addr += bm->pitch;
	}
	dl_jvb(dl, list);
	return length;
}
