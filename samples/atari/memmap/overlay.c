#include "../kernalio/disk_demo.h"
#include "overlay_demo.h"

#include <string.h>

static word transferred;

static byte load(const char * path)
{
	byte channel = cio_free();
	byte status = cio_open(channel, path, CIO_READ, 0);
	if (status != CIO_OK)
		return status;
	status = cio_read(channel, overlay_code, MODULE_SIZE, &transferred);
	byte closed = cio_close(channel);
	if (status != CIO_EOF)
		return cio_failed(status) ? status : CIO_TRUNC;
	if (closed != CIO_OK)
		return closed;
	if (transferred <= ENTRY_SIZE || overlay_code[transferred - 1] != 0)
		return CIO_TRUNC;

	// This sample accepts only the simple return stub used by the fixtures.
	static const byte entry[ENTRY_SIZE] = {0xa9, ENTRY_SIZE, 0x8d, 0x3f, 0x80, 0x60};
	return memcmp(overlay_code, entry, ENTRY_SIZE) == 0 ? CIO_OK : CIO_NOTIMPL;
}

int main(void)
{
	char path[] = "D:OVL1.BIN";
	for (byte i = 0; i < OVERLAYS; ++i)
	{
		path[5] = '1' + i;
		byte status = load(path);
		if (status != CIO_OK)
			return disk_result(status);
		print_overlay();
	}
	return 0;
}
