#include "../kernalio/disk_demo.h"
#include "charset.h"

static byte font[FONT_SIZE];
#pragma align(font, 1024)

static word transferred;

int main(void)
{
	byte channel = cio_free();
	byte status = cio_open(channel, "D:TILES.FNT", CIO_READ, 0);
	if (status != CIO_OK)
		return disk_result(status);
	status = cio_read(channel, font, sizeof(font), &transferred);
	if (!cio_failed(status) && transferred == FONT_SIZE && status != CIO_EOF)
	{
		byte extra;
		status = cio_getc(channel, &extra);
	}
	byte closed = cio_close(channel);
	if (status != CIO_EOF || transferred != FONT_SIZE || closed != CIO_OK)
	{
		con_printf("Provide one 1024-byte ANTIC font as D:TILES.FNT.\n");
		return 1;
	}
	show_font(font);
	return 0;
}
