#include "bitmap_image.h"

static bool fits(word start, word size)
{
	return start && size && start <= 0xffffu - (size - 1);
}

static bool overlaps(word first, word last, word other, word end)
{
	return first <= end && other <= last;
}

static byte validate(const ABitmap * bitmap, const byte * data, word size, ABitmapPalette * palette)
{
	if (!bitmap || bitmap->bits != 2 || !bitmap->height || bitmap->height > 240 ||
		(bitmap->width != 160 && bitmap->width != 192) || bitmap->pitch < bitmap->width / 4)
		return ABM_IMAGE_LAYOUT;
	word pixels = (word)bitmap->data;
	word extent = (word)(bitmap->height - 1) * bitmap->pitch + bitmap->width / 4;
	word input = (word)data, colors = (word)palette, view = (word)bitmap;
	if (!fits(pixels, extent) || overlaps(pixels, pixels + (extent - 1), view, view + sizeof(ABitmap) - 1))
		return ABM_IMAGE_LAYOUT;
	if (!fits(input, size) || overlaps(input, input + (size - 1), pixels, pixels + (extent - 1)) ||
		overlaps(input, input + (size - 1), view, view + sizeof(ABitmap) - 1))
		return ABM_IMAGE_BUFFER;
	if (palette && (!fits(colors, sizeof(ABitmapPalette)) ||
		overlaps(colors, colors + sizeof(ABitmapPalette) - 1, pixels, pixels + (extent - 1)) ||
		overlaps(colors, colors + sizeof(ABitmapPalette) - 1, input, input + (size - 1)) ||
		overlaps(colors, colors + sizeof(ABitmapPalette) - 1, view, view + sizeof(ABitmap) - 1)))
		return ABM_IMAGE_BUFFER;
	return CIO_OK;
}

static void raw_rows(const ABitmap * bitmap, const byte * data, byte width)
{
	volatile byte * row = bitmap->data;
	for (byte y = 0; y < bitmap->height; y++)
	{
		for (byte x = 0; x < width; x++)
			row[x] = data[x];
		data += width;
		row += bitmap->pitch;
	}
}

byte abm_decode_mic(const ABitmap * bitmap, const byte * data, word size, ABitmapPalette * palette)
{
	byte status = validate(bitmap, data, size, palette);
	if (status != CIO_OK)
		return status;
	if (bitmap->width != 160)
		return ABM_IMAGE_LAYOUT;
	word pixels = (word)bitmap->height * 40;
	if (size != pixels && (size < pixels + 4 || size > pixels + 6))
		return ABM_IMAGE_FORMAT;
	raw_rows(bitmap, data, 40);
	if (palette)
	{
		palette->count = size == pixels ? 0 : 4;
		for (byte i = 0; i < 5; i++)
			palette->colors[i] = i < palette->count ? data[pixels + i] : 0;
	}
	return CIO_OK;
}

struct PicTokens
{
	const byte * data;
	word length, offset, count;
	bool literal;
};

static bool next_token(PicTokens * tokens)
{
	if (tokens->offset >= tokens->length)
		return false;
	byte tag = tokens->data[tokens->offset++];
	tokens->literal = (tag & 0x80) != 0;
	word count = tag & 0x7f;
	if (!count)
	{
		if (tokens->length - tokens->offset < 2)
			return false;
		count = (word)tokens->data[tokens->offset] << 8 | tokens->data[tokens->offset + 1];
		tokens->offset += 2;
	}
	tokens->count = count;
	return count && tokens->length - tokens->offset >= (tokens->literal ? count : 1);
}

static word pic_end(const byte * data, word size, word offset, word left)
{
	PicTokens tokens = {data, size, offset, 0, false};
	while (left)
	{
		if (!next_token(&tokens))
			return 0;
		tokens.offset += tokens.literal ? tokens.count : 1;
		if (tokens.count >= left)
			break;
		left -= tokens.count;
	}
	return tokens.offset;
}

static void pic_rows(const ABitmap * bitmap, const PicTokens * input, byte width, bool columns)
{
	PicTokens tokens = *input;
	byte x = 0, y = 0;
	volatile byte * row = bitmap->data;
	word left = (word)width * bitmap->height;
	while (left && next_token(&tokens))
	{
		if (tokens.count > left)
			tokens.count = left;
		left -= tokens.count;
		byte value = 0;
		if (!tokens.literal)
			value = tokens.data[tokens.offset++];
		while (tokens.count--)
		{
			if (tokens.literal)
				value = tokens.data[tokens.offset++];
			row[x] = value;
			if (columns)
			{
				y += 2;
				if (y < bitmap->height)
					row += (word)bitmap->pitch * 2;
				else if (!(y & 1))
				{
					y = 1;
					row = bitmap->data + bitmap->pitch;
				}
				else
				{
					y = 0;
					row = bitmap->data;
					x++;
				}
			}
			else if (++x == width)
			{
				x = 0;
				row += bitmap->pitch;
			}
		}
	}
}

byte abm_decode_pic(const ABitmap * bitmap, const byte * data, word size, ABitmapPalette * palette)
{
	byte status = validate(bitmap, data, size, palette);
	if (status != CIO_OK)
		return status;
	if (size < 26 || data[0] != 0xff || data[1] != 0x80 || data[2] != 0xc9 || data[3] != 0xc7 ||
		data[6] != 1 || data[7] > 2 || data[8] != DL_MODEE || data[9] || data[11])
		return ABM_IMAGE_FORMAT;
	byte width = data[10], height = data[12], compression = data[7];
	if (width != bitmap->width / 4 || height != bitmap->height || (compression == 1 && (height & 1)))
		return ABM_IMAGE_LAYOUT;
	word offset = data[4] | (word)data[5] << 8;
	if (!offset)
		offset = 26;
	if (offset < 26 || offset >= size)
		return ABM_IMAGE_FORMAT;
	word count = (word)width * height;
	if (!compression)
	{
		if (size - offset == count + 1 && data[offset] == 0xa2)
			offset++;
		if (size - offset != count)
			return ABM_IMAGE_FORMAT;
		raw_rows(bitmap, data + offset, width);
	}
	else
	{
		bool has_marker = data[offset] == 0xa2;
		word end = pic_end(data, size, offset, count);
		// Some producers put an A2 marker before the first token. A2 can
		// also be a literal token. Prefer the decode that consumes more input.
		if (has_marker && end != size)
		{
			word marked = pic_end(data, size, offset + 1, count);
			if (marked > end)
			{
				offset++;
				end = marked;
			}
		}
		if (!end)
			return ABM_IMAGE_FORMAT;
		PicTokens input = {data, end, offset, 0, false};
		pic_rows(bitmap, &input, width, compression == 1);
	}
	if (palette)
	{
		palette->count = 5;
		palette->colors[0] = data[17];
		for (byte i = 0; i < 4; i++)
			palette->colors[i + 1] = data[13 + i];
	}
	return CIO_OK;
}

static byte load_image(const ABitmap * bitmap, const ABitmapImageFile * file, ABitmapPalette * palette, bool pic)
{
	if (!file || !file->name)
		return ABM_IMAGE_BUFFER;
	byte status = validate(bitmap, file->work, file->capacity, palette);
	if (status != CIO_OK)
		return status;
	word work = (word)file->work, descriptor = (word)file;
	if (overlaps(work, work + (file->capacity - 1), descriptor, descriptor + sizeof(ABitmapImageFile) - 1))
		return ABM_IMAGE_BUFFER;
	byte channel = cio_free();
	if (channel == 0xff)
		return CIO_TMOF;
	status = cio_open(channel, file->name, CIO_READ, 0);
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
	if (status != CIO_OK && status != CIO_EOF && status != CIO_EOF_NEXT)
		return status;
	if (closed != CIO_OK)
		return closed;
	if (!size)
		return ABM_IMAGE_FORMAT;
	return pic ? abm_decode_pic(bitmap, file->work, size, palette) : abm_decode_mic(bitmap, file->work, size, palette);
}

byte abm_load_mic(const ABitmap * bitmap, const ABitmapImageFile * file, ABitmapPalette * palette)
{
	return load_image(bitmap, file, palette, false);
}

byte abm_load_pic(const ABitmap * bitmap, const ABitmapImageFile * file, ABitmapPalette * palette)
{
	return load_image(bitmap, file, palette, true);
}
