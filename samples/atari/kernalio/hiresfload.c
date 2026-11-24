#include "image_disk.h"

// SAMPLE.RLE consists of count/value pairs. Each pair expands to count
// packed ANTIC bytes; the whole file expands to one complete frame.
#define BUFFER_SIZE 8192
static byte compressed[BUFFER_SIZE];

static bool expand_image(word size)
{
	word output = 0;
	for (word pair = 0; pair + 1 < size; pair += 2)
	{
		byte count = compressed[pair];
		if (!count || count > IMAGE_BYTES - output)
			return false;
		output += count;
	}
	if ((size & 1) || output != IMAGE_BYTES)
		return false;
	volatile byte * pixels = screen.data;
	output = 0;
	for (word pair = 0; pair < size; pair += 2)
	{
		byte count = compressed[pair], value = compressed[pair + 1];
		do
		{
			pixels[output++] = value;
		} while (--count);
	}
	return true;
}

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, "D:BLUMBA2.RLE", CIO_READ, 0);
	if (status != CIO_OK)
		return disk_result(status);
	status = cio_read(channel, compressed, sizeof(compressed), &transferred);
	byte closed = cio_close(channel);
	if (status != CIO_EOF || closed != CIO_OK)
		return disk_result(closed != CIO_OK ? closed : (status < 128 ? CIO_TRUNC : status));
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEE, ROWS) || !expand_image(transferred))
	{
		bitmap_demo_restore(&saved);
		return disk_result(CIO_TRUNC);
	}
	os_color[0] = COLOR_GREEN;
	os_color[1] = COLOR_LIGHTGREEN;
	os_color[2] = COLOR_WHITE;
	os_color[4] = COLOR_GRAY1;
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
