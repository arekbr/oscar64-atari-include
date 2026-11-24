#include "bitmap_font.h"

static bool fits(word start, word size)
{
	return start && start <= 0xffffu - (size - 1);
}

static bool overlap(word a, word n, word b, word m)
{
	return a <= b + (m - 1) && b <= a + (n - 1);
}

static bool storage(const ABitmapFont * font, const byte * glyphs, const byte * widths)
{
	word view = (word)font, data = (word)glyphs, measured = (word)widths;
	return fits(view, sizeof(ABitmapFont)) && fits(data, 1024) && fits(measured, 128) &&
		!overlap(data, 1024, measured, 128) &&
		!overlap(data, 1024, view, sizeof(ABitmapFont)) &&
		!overlap(measured, 128, view, sizeof(ABitmapFont));
}

bool abm_font_prepare(ABitmapFont * font, byte * glyphs, byte * widths, byte spacing)
{
	if (!storage(font, glyphs, widths))
		return false;
	byte * glyph = glyphs;
	for (byte i = 0; i < 128; i++, glyph += 8)
	{
		byte ink = 0;
		for (byte y = 0; y < 8; y++)
			ink |= glyph[y];
		if (!ink)
		{
			widths[i] = 3;
			continue;
		}
		byte first = 0, last = 7;
		for (word mask = 0x80; (ink & mask) == 0; mask >>= 1)
			first++;
		for (word mask = 1; (ink & mask) == 0; mask <<= 1)
			last--;
		widths[i] = last - first + 1;
		if (first)
			for (byte y = 0; y < 8; y++)
				glyph[y] <<= first;
	}
	ABitmapFont prepared = {glyphs, widths, spacing};
	*font = prepared;
	return true;
}

byte abm_font_load(ABitmapFont * font, const ABitmapImageFile * file, byte * widths, byte spacing)
{
	if (!file || !file->name || file->capacity < 1024 || !storage(font, file->work, widths) ||
		overlap((word)file->work, 1024, (word)file, sizeof(ABitmapImageFile)))
		return ABM_IMAGE_BUFFER;
	byte channel = cio_free();
	if (channel == 0xff)
		return CIO_TMOF;
	byte status = cio_open(channel, file->name, CIO_READ, 0);
	if (status != CIO_OK)
		return status;
	word size;
	status = cio_read(channel, file->work, 1024, &size);
	if (status == CIO_OK && size == 1024)
	{
		byte extra;
		status = cio_getc(channel, &extra);
		if (status == CIO_OK || status == CIO_EOF_NEXT)
			status = ABM_IMAGE_FORMAT;
	}
	byte closed = cio_close(channel);
	if (cio_failed(status))
		return status;
	if (closed != CIO_OK)
		return closed;
	if (size != 1024)
		return ABM_IMAGE_FORMAT;
	return abm_font_prepare(font, file->work, widths, spacing) ? CIO_OK : ABM_IMAGE_BUFFER;
}
