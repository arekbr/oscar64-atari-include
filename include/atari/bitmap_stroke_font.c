#include "bitmap_stroke_font.h"

static word read_word(const byte * bytes)
{
	return bytes[0] | ((word)bytes[1] << 8);
}

static bool separate(const ABitmapStrokeFont * font, const byte * data, word size)
{
	word input = (word)data, view = (word)font;
	return input && view && size && input <= 0xffffu - (size - 1) &&
		view <= 0xffffu - sizeof(ABitmapStrokeFont) + 1 &&
		(input + (size - 1) < view || view + sizeof(ABitmapStrokeFont) - 1 < input);
}

static int signed_byte(byte value)
{
	return value <= 127 ? value : (int)value - 256;
}

byte abm_stroke_font_bind(ABitmapStrokeFont * font, const byte * data, word size)
{
	if (!separate(font, data, size))
		return ABM_IMAGE_BUFFER;
	if (size < 34 || data[0] != 'P' || data[1] != 'K' || data[2] != 8 || data[3] != 8)
		return ABM_IMAGE_FORMAT;
	word marker = 4;
	while (marker < size - 2 && data[marker] != 26)
		marker++;
	if (marker >= size - 2)
		return ABM_IMAGE_FORMAT;
	word header = read_word(data + marker + 1);
	if (header < marker + 3 || header > size - 16)
		return ABM_IMAGE_FORMAT;
	const byte * info = data + header;
	word count = read_word(info + 1), relative = read_word(info + 5);
	byte first = info[4];
	if (info[0] != '+' || !count || count > 256u - first ||
		relative < 16 + count * 3 || relative > size - header - 2)
		return ABM_IMAGE_FORMAT;
	const byte * offsets = info + 16;
	const byte * strokes = info + relative;
	word stroke_size = size - header - relative;
	for (word glyph = 0; glyph < count; glyph++)
	{
		word offset = read_word(offsets + 2 * glyph);
		while (offset <= stroke_size - 2 && ((strokes[offset] | strokes[offset + 1]) & 0x80))
			offset += 2;
		if (offset > stroke_size - 2)
			return ABM_IMAGE_FORMAT;
	}
	ABitmapStrokeFont bound = {offsets, offsets + 2 * count, strokes, count,
		stroke_size, first, signed_byte(info[8])};
	*font = bound;
	return CIO_OK;
}

byte abm_stroke_font_load(ABitmapStrokeFont * font, const ABitmapImageFile * file)
{
	if (!file || !file->name || !separate(font, file->work, file->capacity))
		return ABM_IMAGE_BUFFER;
	word work = (word)file->work, desc = (word)file;
	if (work <= desc + sizeof(ABitmapImageFile) - 1 && desc <= work + (file->capacity - 1))
		return ABM_IMAGE_BUFFER;
	byte channel = cio_free();
	if (channel == 0xff)
		return CIO_TMOF;
	byte status = cio_open(channel, file->name, CIO_READ, 0);
	if (status != CIO_OK)
		return status;
	word size;
	status = cio_read(channel, file->work, file->capacity, &size);
	if (status == CIO_OK && size == file->capacity)
	{
		byte extra;
		status = cio_getc(channel, &extra);
		if (status == CIO_OK || status == CIO_EOF_NEXT)
			status = ABM_IMAGE_BUFFER;
	}
	byte closed = cio_close(channel);
	if (cio_failed(status))
		return status;
	if (closed != CIO_OK)
		return closed;
	if (!size)
		return ABM_IMAGE_FORMAT;
	return abm_stroke_font_bind(font, file->work, size);
}

static byte glyph_width(const ABitmapStrokeFont * font, byte code)
{
	word index = (word)code - font->first;
	return index < font->count ? font->widths[index] : 0;
}

static word scale_width(byte width, word scale)
{
	return ((unsigned long)width * scale + 128u) >> 8;
}

word abm_stroke_text_width(const ABitmapStrokeFont * font, const char * text, word scale)
{
	word total = 0;
	while (*text)
	{
		word width = scale_width(glyph_width(font, *text++), scale);
		if (width > 0xffffu - total)
			return 0xffffu;
		total += width;
	}
	return total;
}

static int scale_coord(int coord, word scale)
{
	if (coord < 0)
		return -(int)scale_width(-coord, scale);
	return scale_width(coord, scale);
}

static int coord7(byte value)
{
	value &= 0x7f;
	return value < 64 ? value : (int)value - 128;
}

static int clamp(long value)
{
	if (value < -32768l)
		return -32768;
	if (value > 32767l)
		return 32767;
	return value;
}

struct StrokeGlyph
{
	long x, base;
	word scale;
	const byte * commands;
};

static void draw_glyph(const ABitmapCanvas * canvas, const StrokeGlyph * glyph)
{
	const byte * cmd = glyph->commands;
	int last_x = clamp(glyph->x), last_y = clamp(glyph->base);
	for (;;)
	{
		byte x = cmd[0], y = cmd[1];
		cmd += 2;
		if (!(x & 0x80))
		{
			if (!(y & 0x80))
				return;
			continue;
		}
		int next_x = clamp(glyph->x + scale_coord(coord7(x), glyph->scale));
		int next_y = clamp(glyph->base - scale_coord(coord7(y), glyph->scale));
		if (y & 0x80)
			abm_canvas_line(canvas, last_x, last_y, next_x, next_y);
		last_x = next_x;
		last_y = next_y;
	}
}

void abm_canvas_stroke_text(const ABitmapCanvas * canvas, const ABitmapStrokeFont * font,
	const char * text, word scale)
{
	if (!scale)
		return;
	StrokeGlyph glyph = {canvas->x, (long)canvas->y + scale_coord(font->ascender, scale), scale, 0};
	int margin = scale_width(64, scale);
	while (*text && glyph.x < (long)canvas->right + margin)
	{
		word index = (word)(byte)*text++ - font->first;
		if (index < font->count)
		{
			glyph.commands = font->strokes + read_word(font->offsets + 2 * index);
			if (glyph.x + margin >= canvas->left)
				draw_glyph(canvas, &glyph);
			glyph.x += scale_width(font->widths[index], scale);
		}
	}
}
