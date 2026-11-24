#include "vbxe_image.h"
#include "bitmap_indexed_internal.h"

static bool fits(word start, word count)
{
	return start && count && start <= 0xffffu - (count - 1);
}

static bool overlap(word a, word n, word b, word m)
{
	return a <= b + (m - 1) && b <= a + (n - 1);
}

static bool outside_memac(word start, word count)
{
	return fits(start, count) && !overlap(start, count, VBXE_MEMAC_WINDOW, VBXE_MEMAC_SIZE);
}

static bool valid_surface(const AVbxeImage * image)
{
	if (!outside_memac((word)image, sizeof(AVbxeImage)) ||
		!outside_memac((word)image->board, sizeof(AVbxe)))
		return false;
	const AVbxe * board = image->board;
	if ((board->regs != (volatile byte *)0xd600 && board->regs != (volatile byte *)0xd700) ||
		!(board->revision & 0x7f) || image->palette >= 4 ||
		!image->width || image->width > VBXE_IMAGE_MAX_WIDTH || image->pitch < image->width ||
		!image->height || image->height > VBXE_IMAGE_MAX_HEIGHT || image->pixels >= VBXE_VRAM_SIZE)
		return false;
	unsigned long extent = (unsigned long)(image->height - 1) * image->pitch + image->width;
	if (extent > VBXE_VRAM_SIZE - image->pixels)
		return false;

	return true;
}

static bool valid_work(const AVbxeImage * image, word needed)
{
	word work = (word)image->work;
	return image->capacity >= needed && outside_memac(work, needed) &&
		!overlap(work, needed, (word)image, sizeof(AVbxeImage)) &&
		!overlap(work, needed, (word)image->board, sizeof(AVbxe));
}

static byte check_image(const AVbxeImage * image, byte format, const byte * data, word size, bool from_cio)
{
	if (format < ABM_INDEXED_BMP || format > ABM_INDEXED_GIF)
		return ABM_IMAGE_FORMAT;
	if (!valid_surface(image))
		return ABM_IMAGE_LAYOUT;
	word needed = VBXE_IMAGE_WORK;
	if (format == ABM_INDEXED_GIF)
		needed += ABM_INDEXED_GIF_WORK;
	if (from_cio)
		needed += ABM_INDEXED_CIO_WORK;
	if (!valid_work(image, needed) ||
		(!from_cio && (!fits((word)data, size) || overlap((word)image->work, needed, (word)data, size))))
		return ABM_IMAGE_BUFFER;
	return CIO_OK;
}

bool indexed_write_pixels(const AVbxeImage * image, word x, word y, const byte * pixels, word count)
{
	unsigned long addr = image->pixels + (unsigned long)y * image->pitch + x;
	return vbxe_write(image->board, addr, pixels, count);
}

static byte publish_palette(const IndexedOutput * output, byte status)
{
	if (status == CIO_OK)
	{
		const AVbxeImage * image = output->vbxe;
		vbxe_palette(image->board, image->palette, 0, (const AVbxeColor *)output->palette, output->colors);
	}
	return status;
}

byte vbxe_decode_image(const AVbxeImage * image, byte format, const byte * data, word size)
{
	byte status = check_image(image, format, data, size, false);
	if (status != CIO_OK)
		return status;
	byte * work = image->work;
	IndexedOutput output = {0, image->width, image->height, image, work, work + VBXE_IMAGE_MAX_WIDTH, 0, 0, 0, 0};
	status = indexed_decode(&output, format, data, size, work + VBXE_IMAGE_WORK);
	return publish_palette(&output, status);
}

byte vbxe_load_image(const AVbxeImage * image, byte format, const char * name)
{
	if (!name)
		return CIO_INVFNM;
	byte status = check_image(image, format, 0, 0, true);
	if (status != CIO_OK)
		return status;
	byte * work = image->work;
	IndexedOutput output = {0, image->width, image->height, image, work, work + VBXE_IMAGE_MAX_WIDTH, 0, 0, 0, 0};
	status = indexed_load(&output, format, name, work + VBXE_IMAGE_WORK);
	return publish_palette(&output, status);
}


static void put32(byte * dest, unsigned long value)
{
	dest[0] = value;
	dest[1] = value >> 8;
	dest[2] = value >> 16;
	dest[3] = value >> 24;
}

static byte write_all(byte channel, const byte * data, word size)
{
	word count;
	byte status = cio_write(channel, data, size, &count);
	if (status != CIO_OK)
		return status;
	return count == size ? CIO_OK : CIO_TRUNC;
}

static __noinline byte save_header(byte channel, const AVbxeImage * image, word stride)
{
	static const byte template[54] =
	{
		'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0x36, 4, 0, 0,
		40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 8, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 0, 0, 0, 0, 0, 0
	};
	byte * header = image->work;
	for (byte i = 0; i < sizeof(template); i++)
		header[i] = template[i];
	unsigned long length = (unsigned long)stride * image->height;
	put32(header + 2, 1078ul + length);
	put32(header + 18, image->width);
	put32(header + 22, image->height);
	put32(header + 34, length);
	return write_all(channel, header, sizeof(template));
}

static __noinline byte save_colors(byte channel, byte * work, const AVbxeColor * colors)
{
	for (byte block = 0; block < 4; block++)
	{
		for (byte i = 0; i < 64; i++, colors++)
		{
			word offset = (word)i << 2;
			work[offset] = colors->blue;
			work[offset + 1] = colors->green;
			work[offset + 2] = colors->red;
			work[offset + 3] = 0;
		}
		byte status = write_all(channel, work, 256);
		if (status != CIO_OK)
			return status;
	}
	return CIO_OK;
}

static __noinline byte save_rows(byte channel, const AVbxeImage * image, word stride)
{
	word y = image->height;
	unsigned long addr = image->pixels + (unsigned long)(y - 1) * image->pitch;
	while (y)
	{
		if (!vbxe_read(image->board, addr, image->work, image->width))
			return ABM_IMAGE_LAYOUT;
		for (word i = image->width; i < stride; i++)
			image->work[i] = 0;
		byte status = write_all(channel, image->work, stride);
		if (status != CIO_OK)
			return status;
		y--;
		if (y)
			addr -= image->pitch;
	}
	return CIO_OK;
}

byte vbxe_save_bmp(const AVbxeImage * image, const AVbxeColor * colors, const char * name)
{
	if (!name)
		return CIO_INVFNM;
	if (!valid_surface(image))
		return ABM_IMAGE_LAYOUT;
	if (!colors)
		colors = vbxe_palette_entries(image->board, image->palette, 0, 256);
	word stride = (image->width + 3u) & ~3u;
	word needed = stride > 256 ? stride : 256;
	word color_size = sizeof(AVbxeColor) * 256u;
	if (!valid_work(image, needed) || !fits((word)colors, color_size) ||
		overlap((word)image->work, needed, (word)colors, color_size))
		return ABM_IMAGE_BUFFER;

	byte channel = cio_free();
	if (channel == 0xff)
		return CIO_TMOF;
	byte status = cio_open(channel, name, CIO_WRITE, 0);
	if (status != CIO_OK)
		return status;
	status = save_header(channel, image, stride);
	if (status == CIO_OK)
		status = save_colors(channel, image->work, colors);
	if (status == CIO_OK)
		status = save_rows(channel, image, stride);
	byte closed = cio_close(channel);
	return status == CIO_OK ? closed : status;
}
