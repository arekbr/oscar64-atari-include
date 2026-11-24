#include "image_disk.h"

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, IMAGE_PATH, CIO_READ, 0);
	if (status != CIO_OK)
		return disk_result(status);

	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_mono_image())
	{
		cio_close(channel);
		return 1;
	}
	// The scanlines are contiguous in both ANTIC memory and the file.
	status = cio_read(channel, (void *)screen.data, IMAGE_BYTES, &transferred);
	byte closed = cio_close(channel);
	if (!cio_failed(status) && closed == CIO_OK && transferred == IMAGE_BYTES)
		key_get();
	bitmap_demo_restore(&saved);
	if (cio_failed(status))
		return disk_result(status);
	if (transferred != IMAGE_BYTES)
		return disk_result(CIO_TRUNC);
	return disk_result(closed);
}
