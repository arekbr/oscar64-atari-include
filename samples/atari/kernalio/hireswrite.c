#include "image_disk.h"

// Integer square root for positioning the circular brush along the smile.
static byte root(word square)
{
	byte r = 0;
	while ((word)(r + 1) * (r + 1) <= square)
		++r;
	return r;
}

static void draw_image(void)
{
	abm_fill_ellipse(&screen, 160, 100, 90, 90, 1);
	abm_fill_ellipse(&screen, 120, 80, 20, 20, 0);
	abm_fill_ellipse(&screen, 200, 80, 20, 20, 0);
	for (int x = -40; x <= 40; x += 4)
		abm_fill_ellipse(&screen, 160 + x, 107 + root(2500 - x * x), 6, 6, 0);
}

int main(void)
{
	byte channel = cio_free();
	byte status = open_new(channel, IMAGE_PATH);
	if (status != CIO_OK)
		return disk_result(status);

	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_mono_image())
	{
		cio_close(channel);
		return 1;
	}
	draw_image();

	for (word i = 0; i < IMAGE_BYTES; i += 200)
	{
		status = cio_write(channel, (const void *)(screen.data + i), 200, &transferred);
		if (status != CIO_OK || transferred != 200)
			break;
	}
	byte closed = cio_close(channel);
	bitmap_demo_restore(&saved);
	if (status != CIO_OK)
		return disk_result(status);
	if (transferred != 200)
		return disk_result(CIO_TRUNC);
	return disk_result(closed);
}
