#include <atari/cio.h>
#include <atari/console.h>

// Boot with DOS and put MESSAGE.TXT on D:. Use ATASCII line endings ($9B).
// Write the bytes directly to E: so the screen editor handles the line endings.
static byte copy_to_screen(byte channel)
{
	static byte buffer[64];

	for (;;)
	{
		word count;
		byte status = cio_read(channel, buffer, sizeof(buffer), &count);
		if (cio_failed(status))
			return status;

		// A read that reaches EOF can still have bytes to display.
		if (count)
		{
			word written;
			byte screen = cio_write(0, buffer, count, &written);
			if (cio_failed(screen))
				return screen;
		}
		if (status == CIO_EOF || status == CIO_EOF_NEXT || count == 0)
			return CIO_OK;
	}
}

int main(void)
{
	con_clrscr();
	byte channel = cio_free();
	if (channel == 0xff)
		con_printf("No free CIO channel\n");
	else
	{
		byte status = cio_open(channel, "D:MESSAGE.TXT", CIO_READ, 0);
		if (cio_failed(status))
		{
			cio_close(channel);
			con_printf("Open failed (%u)\n", (unsigned)status);
		}
		else
		{
			status = copy_to_screen(channel);
			byte closed = cio_close(channel);
			if (cio_failed(status))
				con_printf("Copy failed (%u)\n", (unsigned)status);
			else if (cio_failed(closed))
				con_printf("Close failed (%u)\n", (unsigned)closed);
			else
				con_printf("\nEnd of file\n");
		}
	}
	return 0;
}
