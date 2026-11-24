#include "bitmap_canvas.h"

struct Blit
{
	const ABitmap * dst;
	const ABitmapImage * src;
	int dx, dy, sx, sy;
	word width;
	byte height, key;
	bool same;
};

static bool clip_axis(int * dst, int * src, word * length, word dst_size, word src_size)
{
	word skip = 0;
	if (*dst < 0)
		skip = -(long)*dst;
	if (*src < 0 && -(long)*src > skip)
		skip = -(long)*src;
	if (skip >= *length)
		return false;

	long d = (long)*dst + skip;
	long s = (long)*src + skip;
	if (d >= dst_size || s >= src_size)
		return false;
	word n = *length - skip;
	if (n > dst_size - (word)d)
		n = dst_size - (word)d;
	if (n > src_size - (word)s)
		n = src_size - (word)s;
	*dst = d;
	*src = s;
	*length = n;
	return n != 0;
}

static word image_extent(word width, byte height, byte pitch, byte bits)
{
	if (!width || !height || (bits != 1 && bits != 2 && bits != 4) ||
		width > (word)pitch * (8 / bits))
		return 0;
	return (word)(height - 1) * pitch + ((width * bits + 7) >> 3);
}

struct BlitSpan
{
	byte src_bytes, dst_bytes;
	byte left_mask, right_mask;
	byte shift, carry_shift, borrow;
	byte bits, key, packed_key;
	bool reverse;
};

static void copy_row(volatile byte * dst, const volatile byte * src, byte n, bool reverse)
{
	if (reverse)
	{
		while (n)
		{
			n--;
			dst[n] = src[n];
		}
	}
	else
	{
		for (byte i = 0; i < n; i++)
			dst[i] = src[i];
	}
}

static byte packed_color(byte bits, byte key)
{
	if (bits == 1)
		return key ? 0xff : 0;
	if (bits == 2)
		return key * 0x55;
	return (key << 4) | key;
}

static __forceinline byte opaque_mask(byte diff, byte bits)
{
	// Expand the nonzero bits of each pixel to that pixel's entire field.
	if (bits == 1)
		return diff;
	if (bits == 2)
	{
		byte pixels = (diff | (diff >> 1)) & 0x55;
		return pixels | (pixels << 1);
	}
	diff |= diff >> 2;
	diff = (diff | (diff >> 1)) & 0x11;
	return (diff << 4) - diff;
}

static void blit_row(volatile byte * dst, const volatile byte * src, const BlitSpan * span)
{
	byte last = span->dst_bytes - 1;
	byte index = span->reverse ? last : 0;
	sbyte step = span->reverse ? -1 : 1;

	for (byte left = span->dst_bytes; left; left--)
	{
		// An index of 255 can only be the byte before the source span.
		// Do not fetch bytes that contain no source pixels for this copy.
		byte first = index - span->borrow;
		byte value = 0;
		if (first < span->src_bytes)
			value = src[first] << span->shift;
		byte next = first + 1;
		if (span->shift && next < span->src_bytes)
			value |= src[next] >> span->carry_shift;

		byte mask = index ? 0xff : span->left_mask;
		if (index == last)
			mask &= span->right_mask;
		if (span->key != ABM_OPAQUE)
			mask &= opaque_mask(value ^ span->packed_key, span->bits);
		if (mask == 0xff)
			dst[index] = value;
		else if (mask)
			dst[index] = (dst[index] & ~mask) | (value & mask);
		index += step;
	}
}

static void blit_rows(const Blit * blit, byte height)
{
	const ABitmap * dst = blit->dst;
	const ABitmapImage * src = blit->src;
	byte bits = src->bits;
	word from = (word)blit->sx * bits;
	word to = (word)blit->dx * bits;
	word length = blit->width * bits;
	word from_end = from + length - 1;
	word to_end = to + length - 1;
	byte from_phase = from & 7;
	byte to_phase = to & 7;

	BlitSpan span;
	span.src_bytes = (from_end >> 3) - (from >> 3) + 1;
	span.dst_bytes = (to_end >> 3) - (to >> 3) + 1;
	span.left_mask = 0xff >> to_phase;
	span.right_mask = 0xff << (7 - (to_end & 7));
	span.shift = (from_phase - to_phase) & 7;
	span.carry_shift = 8 - span.shift;
	span.borrow = from_phase < to_phase;
	span.bits = bits;
	span.key = blit->key;
	span.packed_key = packed_color(bits, blit->key);
	span.reverse = blit->same && blit->dy == blit->sy && blit->dx > blit->sx;
	bool raw = span.key == ABM_OPAQUE && !span.shift &&
		span.left_mask == 0xff && span.right_mask == 0xff;

	word sy = blit->sy, dy = blit->dy;
	int src_step = src->pitch, dst_step = dst->pitch;
	if (blit->same && dy > sy)
	{
		sy += height - 1;
		dy += height - 1;
		src_step = -src_step;
		dst_step = -dst_step;
	}
	const volatile byte * in = src->data + sy * src->pitch + (from >> 3);
	volatile byte * out = dst->data + dy * dst->pitch + (to >> 3);
	for (;;)
	{
		if (raw)
			copy_row(out, in, span.dst_bytes, span.reverse);
		else
			blit_row(out, in, &span);
		if (!--height)
			return;
		in += src_step;
		out += dst_step;
	}
}

static bool blit_rect(Blit * blit, bool allow_same)
{
	const ABitmap * dst = blit->dst;
	const ABitmapImage * src = blit->src;
	if (dst->bits != src->bits)
		return false;
	word src_size = image_extent(src->width, src->height, src->pitch, src->bits);
	word dst_size = image_extent(dst->width, dst->height, dst->pitch, dst->bits);
	word s = (word)src->data, d = (word)dst->data;
	if (!s || !d || !src_size || !dst_size || src_size > src->size ||
		s > 0xffffu - (src_size - 1) || d > 0xffffu - (dst_size - 1) ||
		(blit->key != ABM_OPAQUE && blit->key >= (1 << src->bits)))
		return false;

	blit->same = allow_same && s == d && src->pitch == dst->pitch &&
		src->width == dst->width && src->height == dst->height;
	if (!blit->same && s <= d + (dst_size - 1) && d <= s + (src_size - 1))
		return false;

	word height = blit->height;
	if (!clip_axis(&blit->dx, &blit->sx, &blit->width, dst->width, src->width) ||
		!clip_axis(&blit->dy, &blit->sy, &height, dst->height, src->height))
		return true;

	blit_rows(blit, height);
	return true;
}

bool abm_copy_rect(const ABitmap * dst, int x, int y, const ABitmap * src, const ABitmapRect * rect)
{
	if (!dst || !src || !rect)
		return false;
	ABitmap target = *dst;
	ABitmapImage image = {src->data, 0xffffu, src->width, src->height, src->pitch, src->bits};
	Blit blit = {&target, &image, x, y, rect->x, rect->y, rect->width, rect->height, ABM_OPAQUE, false};
	return blit_rect(&blit, true);
}

bool abm_blit(const ABitmap * dst, int x, int y, const ABitmapImage * image, byte key)
{
	if (!dst || !image)
		return false;
	ABitmap target = *dst;
	ABitmapImage source = *image;
	Blit blit = {&target, &source, x, y, 0, 0, source.width, source.height, key, false};
	return blit_rect(&blit, false);
}

static word clip_to_window(int * dst, int * src, word length, word first, word last)
{
	if (first == last || (long)*dst >= last)
		return 0;
	if ((long)*dst < first)
	{
		word skip = (long)first - *dst;
		long source = (long)*src + skip;
		if (skip >= length || source > 32767L)
			return 0;
		*dst = first;
		*src = source;
		length -= skip;
	}
	word space = last - (word)*dst;
	return length < space ? length : space;
}

static void clip_to_canvas(Blit * blit, const ABitmapCanvas * canvas)
{
	blit->width = clip_to_window(&blit->dx, &blit->sx, blit->width, canvas->left, canvas->right);
	blit->height = clip_to_window(&blit->dy, &blit->sy, blit->height, canvas->top, canvas->bottom);
}

bool abm_canvas_copy_rect(const ABitmapCanvas * canvas, int x, int y, const ABitmap * src, const ABitmapRect * rect)
{
	if (!canvas || !src || !rect)
		return false;
	ABitmap target = *canvas->bitmap;
	ABitmapImage image = {src->data, 0xffffu, src->width, src->height, src->pitch, src->bits};
	Blit blit = {&target, &image, x, y, rect->x, rect->y, rect->width, rect->height, ABM_OPAQUE, false};
	clip_to_canvas(&blit, canvas);
	return blit_rect(&blit, true);
}

bool abm_canvas_blit(const ABitmapCanvas * canvas, int x, int y, const ABitmapImage * image, byte key)
{
	if (!canvas || !image)
		return false;
	ABitmap target = *canvas->bitmap;
	ABitmapImage source = *image;
	Blit blit = {&target, &source, x, y, 0, 0, source.width, source.height, key, false};
	clip_to_canvas(&blit, canvas);
	return blit_rect(&blit, false);
}
