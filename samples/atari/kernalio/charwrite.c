#include "disk_demo.h"

int main(void)
{
	byte channel = cio_free();
	byte status = open_new(channel, "D:CHARS.DAT");
	if (status != CIO_OK)
		return disk_result(status);
	for (byte value = 0; value < 128 && status == CIO_OK; ++value)
		status = cio_putc(channel, value);
	byte closed = cio_close(channel);
	if (status == CIO_OK && closed == CIO_OK)
		con_printf("Wrote CHARS.DAT\n");
	return disk_result(status == CIO_OK ? closed : status);
}
