#include "disk_demo.h"

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, "D:CHARS.DAT", CIO_READ, 0);
	if (status != CIO_OK)
		return disk_result(status);
	word index = 0;
	byte value;
	while ((status = cio_getc(channel, &value)) < 128)
		con_printf("%u : %u\n", index++, value);
	byte closed = cio_close(channel);
	return disk_result(status == CIO_EOF ? closed : status);
}
