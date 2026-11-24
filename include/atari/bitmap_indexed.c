#include "bitmap_indexed_internal.h"

struct IndexedReader
{
	const byte * next;
	word left;
	byte * block;
	byte channel, status;
	bool last;
};

static bool available(const IndexedReader * reader)
{
	return reader->status == CIO_OK;
}

static void refill(IndexedReader * reader)
{
	if (reader->last)
	{
		reader->status = ABM_IMAGE_FORMAT;
		return;
	}
	word size;
	byte status = cio_read(reader->channel, reader->block, ABM_INDEXED_CIO_WORK, &size);
	if (cio_failed(status))
	{
		reader->status = status;
		return;
	}
	reader->last = status != CIO_OK || size < ABM_INDEXED_CIO_WORK;
	reader->next = reader->block;
	reader->left = size;
	if (!size)
		reader->status = ABM_IMAGE_FORMAT;
}

static __noinline byte read_byte(IndexedReader * reader)
{
	if (!reader->left && available(reader))
		refill(reader);
	if (!available(reader))
		return 0;
	reader->left--;
	return *reader->next++;
}

static void read_bytes(IndexedReader * reader, byte * dest, byte size)
{
	for (byte i = 0; i < size && available(reader); i++)
		dest[i] = read_byte(reader);
}

static word read_word(const byte * data)
{
	return data[0] | ((word)data[1] << 8);
}

static unsigned long read_dword(const byte * data)
{
	return read_word(data) | ((unsigned long)read_word(data + 2) << 16);
}

static void skip(IndexedReader * reader, word count)
{
	while (count && available(reader))
	{
		if (!reader->left)
			refill(reader);
		word part = reader->left < count ? reader->left : count;
		reader->left -= part;
		reader->next += part;
		count -= part;
	}
}

struct IndexedArea
{
	word x, y, width, height;
};

static bool within(const IndexedOutput * output, const IndexedArea * area)
{
	return area->width && area->height && area->x < output->width && area->y < output->height &&
		area->width <= output->width - area->x && area->height <= output->height - area->y;
}

static __noinline byte flush_pixels(IndexedOutput * output)
{
	word count = output->pending;
	if (!count)
		return CIO_OK;
	output->pending = 0;
	return indexed_write_pixels(output->vbxe, output->x, output->y, output->pixels, count)
		? CIO_OK : ABM_IMAGE_LAYOUT;
}

static __noinline byte put_index(IndexedOutput * output, word x, word y, byte index)
{
	if (output->bitmap)
	{
		const ABitmapIndexedImage * image = output->bitmap;
		abm_put(image->bitmap, x, y, image->colors ? image->colors[index] : index);
	}
	else
	{
		if (index >= output->colors)
			return ABM_IMAGE_FORMAT;
		word pending = output->pending;
		if (pending && (y != output->y || x != output->x + pending))
		{
			byte status = flush_pixels(output);
			if (status != CIO_OK)
				return status;
			pending = 0;
		}
		if (!pending)
		{
			output->x = x;
			output->y = y;
		}
		output->pixels[pending] = index;
		output->pending = pending + 1;
	}
	return CIO_OK;
}

static void read_palette(IndexedReader * reader, IndexedOutput * output, word colors)
{
	if (output->palette)
	{
		for (word i = 0; i < 3u * colors && available(reader); i++)
			output->palette[i] = read_byte(reader);
		output->colors = colors;
	}
	else
		skip(reader, 3u * colors);
}

static void bmp_palette(IndexedReader * reader, IndexedOutput * output, word header_end, word start, byte depth)
{
	byte fields[16];
	read_bytes(reader, fields, sizeof(fields));
	if (!available(reader))
		return;
	unsigned long declared = read_dword(fields + 12);
	word max = 1u << depth;
	if (declared > max)
	{
		reader->status = ABM_IMAGE_FORMAT;
		return;
	}
	word colors = declared ? (word)declared : max;
	word bytes = 4u * colors;
	if (bytes > start - header_end)
	{
		reader->status = ABM_IMAGE_FORMAT;
		return;
	}
	skip(reader, header_end - 50);
	byte * rgb = output->palette;
	for (word i = 0; i < colors && available(reader); i++, rgb += 3)
	{
		rgb[2] = read_byte(reader);
		rgb[1] = read_byte(reader);
		rgb[0] = read_byte(reader);
		read_byte(reader);
	}
	skip(reader, start - header_end - bytes);
	output->colors = colors;
}

static byte decode_bmp(IndexedOutput * output, IndexedReader * reader)
{
	byte header_bytes[34];
	read_bytes(reader, header_bytes, sizeof(header_bytes));
	if (!available(reader))
		return reader->status;
	if (read_word(header_bytes) != 0x4d42)
		return ABM_IMAGE_FORMAT;
	unsigned long start = read_dword(header_bytes + 10);
	unsigned long header = read_dword(header_bytes + 14);
	unsigned long width = read_dword(header_bytes + 18);
	long signed_height = (long)read_dword(header_bytes + 22);
	word planes = read_word(header_bytes + 26);
	word depth = read_word(header_bytes + 28);
	unsigned long compression = read_dword(header_bytes + 30);
	if (header < 40 || header > 65521ul || start > 65535ul || start < 14 + header ||
		planes != 1 || compression || (depth != 1 && depth != 4 && depth != 8))
		return ABM_IMAGE_FORMAT;
	if (!width || width > output->width || !signed_height ||
		signed_height < -(long)output->height || signed_height > output->height)
		return ABM_IMAGE_LAYOUT;
	bool top_down = signed_height < 0;
	word height = top_down ? -signed_height : signed_height;
	word row_bytes = ((word)width * (byte)depth + 7) >> 3;
	byte padding = (4 - (row_bytes & 3)) & 3;
	byte mask = (1 << depth) - 1;
	if (output->palette)
		bmp_palette(reader, output, (word)(14 + header), (word)start, depth);
	else
		skip(reader, (word)start - 34);
	for (word row = 0; row < height && available(reader); row++)
	{
		word y = top_down ? row : height - 1 - row;
		byte packed = 0, bits = 0;
		for (word x = 0; x < width && available(reader); x++)
		{
			if (!bits)
			{
				packed = read_byte(reader);
				bits = 8;
			}
			bits -= depth;
			if (available(reader))
				reader->status = put_index(output, x, y, (packed >> bits) & mask);
		}
		skip(reader, padding);
	}
	return reader->status;
}

struct PcxRun
{
	byte value, count;
	bool encoded;
};

static byte pcx_byte(IndexedReader * reader, PcxRun * run)
{
	if (!run->count)
	{
		byte value = read_byte(reader);
		run->count = 1;
		if (run->encoded && (value & 0xc0) == 0xc0)
		{
			run->count = value & 0x3f;
			value = read_byte(reader);
			if (!run->count && available(reader))
				reader->status = ABM_IMAGE_FORMAT;
		}
		run->value = value;
	}
	if (run->count)
		run->count--;
	return run->value;
}

static byte decode_pcx(IndexedOutput * output, IndexedReader * reader)
{
	byte header[68];
	read_bytes(reader, header, sizeof(header));
	skip(reader, 60);
	if (!available(reader))
		return reader->status;
	byte manufacturer = header[0], version = header[1], encoding = header[2], bits = header[3];
	word left = read_word(header + 4), top = read_word(header + 6);
	word right = read_word(header + 8), bottom = read_word(header + 10);
	byte planes = header[65];
	word stride = read_word(header + 66);
	if (manufacturer != 10 || version > 5 || encoding > 1 || planes != 1 ||
		(bits != 1 && bits != 2 && bits != 4 && bits != 8))
		return ABM_IMAGE_FORMAT;
	if (right < left || right >= output->width || bottom < top || bottom >= output->height)
		return ABM_IMAGE_LAYOUT;
	word width = right - left + 1;
	word row_bytes = (width * bits + 7) >> 3;
	if (stride < row_bytes)
		return ABM_IMAGE_FORMAT;
	byte mask = (1 << bits) - 1;
	PcxRun run = {0, 0, encoding != 0};
	if (output->palette)
	{
		if (bits == 8 && version != 5)
			return ABM_IMAGE_FORMAT;
		if (bits != 8 && version == 3)
			return ABM_IMAGE_FORMAT;
		output->colors = 1u << bits;
		if (bits != 8)
			for (byte i = 0; i < 3u * output->colors; i++)
				output->palette[i] = header[16 + i];
	}
	for (word y = top; y <= bottom && available(reader); y++)
	{
		byte packed = 0, remaining = 0;
		for (word x = left; x <= right && available(reader); x++)
		{
			if (!remaining)
			{
				packed = pcx_byte(reader, &run);
				remaining = 8;
			}
			remaining -= bits;
			if (available(reader))
				reader->status = put_index(output, x, y, (packed >> remaining) & mask);
		}
		for (word pad = row_bytes; pad < stride && available(reader); pad++)
			pcx_byte(reader, &run);
	}
	if (available(reader))
	{
		if (run.count)
			return ABM_IMAGE_FORMAT;
		if (output->palette && bits == 8)
		{
			if (read_byte(reader) != 0x0c)
				return ABM_IMAGE_FORMAT;
			read_palette(reader, output, 256);
		}
	}
	return reader->status;
}

struct GifTable
{
	word prefix[4096];
	byte suffix[4096], stack[4096];
};

struct GifStream
{
	unsigned long bits;
	byte count, length;
};

static word gif_code(IndexedReader * reader, GifStream * stream, byte size)
{
	while (stream->count < size && available(reader))
	{
		if (!stream->length)
		{
			stream->length = read_byte(reader);
			if (!stream->length && available(reader))
				reader->status = ABM_IMAGE_FORMAT;
		}
		byte data = read_byte(reader);
		stream->bits |= (unsigned long)data << stream->count;
		stream->count += 8;
		if (stream->length)
			stream->length--;
	}
	word code = stream->bits & ((1u << size) - 1u);
	stream->bits >>= size;
	stream->count -= size;
	return code;
}

static void gif_skip_blocks(IndexedReader * reader)
{
	byte count = read_byte(reader);
	while (count && available(reader))
	{
		skip(reader, count);
		count = read_byte(reader);
	}
}

struct GifPicture
{
	IndexedArea area;
	word x, row, colors;
	byte pass, transparent;
	bool interlaced, masked;
};

static byte gif_put(IndexedOutput * output, GifPicture * picture, byte color)
{
	if (picture->row >= picture->area.height || color >= picture->colors)
		return ABM_IMAGE_FORMAT;
	if (!picture->masked || color != picture->transparent)
	{
		byte status = put_index(output, picture->area.x + picture->x, picture->area.y + picture->row, color);
		if (status != CIO_OK)
			return status;
	}
	if (++picture->x == picture->area.width)
	{
		picture->x = 0;
		if (!picture->interlaced)
			picture->row++;
		else
		{
			static const word first[5] = {0, 4, 2, 1, 256};
			static const byte step[4] = {8, 8, 4, 2};
			word next = (word)picture->row + step[picture->pass];
			while (next >= picture->area.height && picture->pass < 4)
				next = first[++picture->pass];
			picture->row = next;
		}
	}
	return CIO_OK;
}

static __noinline byte gif_string(IndexedOutput * output, GifPicture * picture, GifTable * table,
	word code, word clear, byte * first)
{
	word length = 0;
	while (code >= clear)
	{
		table->stack[length++] = table->suffix[code];
		code = table->prefix[code];
	}
	*first = code;
	byte status = gif_put(output, picture, code);
	while (length && status == CIO_OK)
		status = gif_put(output, picture, table->stack[--length]);
	return status;
}

// Keep the LZW state separate from the caller to fit Oscar64's zero page.
static __noinline byte gif_lzw(IndexedOutput * output, IndexedReader * reader, GifPicture * picture, GifTable * table)
{
	byte base = read_byte(reader);
	if (!available(reader))
		return reader->status;
	if (base < 2 || base > 8)
		return ABM_IMAGE_FORMAT;
	word clear = 1u << base, eoi = clear + 1;
	word next = eoi + 1, previous = 0;
	byte size = base + 1, first = 0;
	bool have_previous = false;
	GifStream stream = {0, 0, 0};
	while (available(reader))
	{
		word code = gif_code(reader, &stream, size);
		if (!available(reader))
			break;
		if (code == clear)
		{
			next = eoi + 1;
			size = base + 1;
			have_previous = false;
		}
		else if (code == eoi)
		{
			if (picture->row < picture->area.height)
				return ABM_IMAGE_FORMAT;
			skip(reader, stream.length);
			gif_skip_blocks(reader);
			return reader->status;
		}
		else
		{
			if (code > next || (code >= clear && !have_previous))
				return ABM_IMAGE_FORMAT;
			bool repeat = code == next;
			word string = repeat ? previous : code;
			byte status = gif_string(output, picture, table, string, clear, &first);
			if (status == CIO_OK && repeat)
				status = gif_put(output, picture, first);
			if (status != CIO_OK)
				return status;
			if (have_previous && next < 4096u)
			{
				table->prefix[next] = previous;
				table->suffix[next++] = first;
				if (size < 12 && next == (1u << size))
					size++;
			}
			previous = code;
			have_previous = true;
		}
	}
	return reader->status;
}

static word gif_palette(IndexedReader * reader, IndexedOutput * output, byte packed)
{
	if (!(packed & 0x80))
		return 0;
	word colors = 2u << (packed & 7);
	read_palette(reader, output, colors);
	return colors;
}

static byte gif_frame(IndexedReader * reader, GifPicture * picture)
{
	for (;;)
	{
		byte block = read_byte(reader);
		if (!available(reader))
			return reader->status;
		if (block == ',')
			return CIO_OK;
		if (block != '!')
			return ABM_IMAGE_FORMAT;
		byte kind = read_byte(reader);
		if (kind != 0xf9)
		{
			gif_skip_blocks(reader);
			if (kind == 1)
				picture->masked = false;
		}
		else
		{
			if (read_byte(reader) != 4)
				return ABM_IMAGE_FORMAT;
			picture->masked = (read_byte(reader) & 1) != 0;
			skip(reader, 2);
			picture->transparent = read_byte(reader);
			if (read_byte(reader))
				return ABM_IMAGE_FORMAT;
		}
	}
}

static byte decode_gif(IndexedOutput * output, IndexedReader * reader, GifTable * table)
{
	byte header[13];
	read_bytes(reader, header, sizeof(header));
	if (!available(reader))
		return reader->status;
	if (read_word(header) != 0x4947 || read_word(header + 2) != 0x3846 ||
		(header[4] != '7' && header[4] != '9') || header[5] != 'a')
		return ABM_IMAGE_FORMAT;
	word width = read_word(header + 6), height = read_word(header + 8);
	byte packed = header[10];
	GifPicture picture;
	picture.colors = gif_palette(reader, output, packed);
	picture.masked = false;
	byte status = gif_frame(reader, &picture);
	if (status != CIO_OK)
		return status;
	byte frame[9];
	read_bytes(reader, frame, sizeof(frame));
	if (!available(reader))
		return reader->status;
	word x = read_word(frame), y = read_word(frame + 2);
	word w = read_word(frame + 4), h = read_word(frame + 6);
	packed = frame[8];
	if (!width || !height || y >= height || h > height - y || x >= width || w > width - x ||
		y >= output->height || h > output->height - y)
		return ABM_IMAGE_LAYOUT;
	picture.area.x = x;
	picture.area.y = y;
	picture.area.width = w;
	picture.area.height = h;
	if (!within(output, &picture.area))
		return ABM_IMAGE_LAYOUT;
	word local = gif_palette(reader, output, packed);
	if (local)
		picture.colors = local;
	if (!picture.colors)
		return ABM_IMAGE_FORMAT;
	picture.interlaced = (packed & 0x40) != 0;
	picture.x = 0;
	picture.row = 0;
	picture.pass = 0;
	return gif_lzw(output, reader, &picture, table);
}

static bool fits(word address, word size)
{
	return address && size && address <= 0xffffu - (size - 1);
}

static bool overlap(word a, word n, word b, word m)
{
	return a <= b + (m - 1) && b <= a + (n - 1);
}

static byte check_image(const ABitmapIndexedImage * image, byte format, const byte * data, word size, bool from_cio)
{
	if (format < ABM_INDEXED_BMP || format > ABM_INDEXED_GIF)
		return ABM_IMAGE_FORMAT;
	if (!fits((word)image, sizeof(ABitmapIndexedImage)) || !fits((word)image->bitmap, sizeof(ABitmap)))
		return ABM_IMAGE_LAYOUT;
	const ABitmap * bitmap = image->bitmap;
	byte bits = bitmap->bits, height = bitmap->height;
	if ((bits != 1 && bits != 2 && bits != 4) || !height || height > 240 ||
		!bitmap->width || bitmap->width > (word)bitmap->pitch * (8 / bits))
		return ABM_IMAGE_LAYOUT;
	word pixels = (word)bitmap->data;
	word extent = (word)(height - 1) * bitmap->pitch + ((bitmap->width * bits + 7) >> 3);
	word view = (word)bitmap, config = (word)image, colors = (word)image->colors;
	if (!fits(pixels, extent) || overlap(pixels, extent, view, sizeof(ABitmap)) ||
		overlap(pixels, extent, config, sizeof(ABitmapIndexedImage)))
		return ABM_IMAGE_LAYOUT;
	word input = (word)data, work = (word)image->work;
	word needed = format == ABM_INDEXED_GIF ? ABM_INDEXED_GIF_WORK : 0;
	if (from_cio)
		needed += ABM_INDEXED_CIO_WORK;
	else if (!fits(input, size) || overlap(input, size, pixels, extent))
		return ABM_IMAGE_BUFFER;
	if (colors && (!fits(colors, 256) || overlap(colors, 256, pixels, extent)))
		return ABM_IMAGE_BUFFER;
	if (needed && (image->capacity < needed || !fits(work, needed) ||
		overlap(work, needed, pixels, extent) || overlap(work, needed, view, sizeof(ABitmap)) ||
		overlap(work, needed, config, sizeof(ABitmapIndexedImage)) ||
		(colors && overlap(work, needed, colors, 256)) ||
		(!from_cio && overlap(work, needed, input, size))))
		return ABM_IMAGE_BUFFER;
	return CIO_OK;
}

static __noinline byte decode_indexed(IndexedOutput * output, byte format, IndexedReader * reader, GifTable * table)
{
	byte status;
	switch (format)
	{
	case ABM_INDEXED_BMP: status = decode_bmp(output, reader); break;
	case ABM_INDEXED_PCX: status = decode_pcx(output, reader); break;
	default: status = decode_gif(output, reader, table); break;
	}
	if (!available(reader))
		return reader->status;
	return status == CIO_OK ? flush_pixels(output) : status;
}

byte indexed_decode(IndexedOutput * output, byte format, const byte * data, word size, byte * work)
{
	IndexedReader reader = {data, size, 0, 0, CIO_OK, true};
	return decode_indexed(output, format, &reader, (GifTable *)work);
}

byte indexed_load(IndexedOutput * output, byte format, const char * name, byte * work)
{
	if (!name)
		return CIO_INVFNM;
	byte channel = cio_free();
	if (channel == 0xff)
		return CIO_TMOF;
	byte status = cio_open(channel, name, CIO_READ, 0);
	if (status != CIO_OK)
		return status;
	IndexedReader reader = {0, 0, work, channel, CIO_OK, false};
	status = decode_indexed(output, format, &reader, (GifTable *)(work + ABM_INDEXED_CIO_WORK));
	byte closed = cio_close(channel);
	return status == CIO_OK ? closed : status;
}

__noinline byte abm_decode_indexed(const ABitmapIndexedImage * image, byte format, const byte * data, word size)
{
	byte status = check_image(image, format, data, size, false);
	if (status != CIO_OK)
		return status;
	IndexedOutput output = {image, image->bitmap->width, image->bitmap->height, 0, 0, 0, 0, 0, 0, 0};
	return indexed_decode(&output, format, data, size, image->work);
}

__noinline byte abm_load_indexed(const ABitmapIndexedImage * image, byte format, const char * name)
{
	byte status = check_image(image, format, 0, 0, true);
	if (status != CIO_OK)
		return status;
	IndexedOutput output = {image, image->bitmap->width, image->bitmap->height, 0, 0, 0, 0, 0, 0, 0};
	return indexed_load(&output, format, name, image->work);
}
