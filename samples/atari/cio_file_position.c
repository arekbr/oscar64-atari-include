#include <atari/cio.h>
#include <atari/console.h>

#pragma section(startup, 0)
#pragma region(startup, 0x3000, 0x3080, , , {startup})
#pragma region(main, 0x3080, 0x8000, , , {code, data, bss, heap, stack})

int main(void)
{
	byte ch = cio_free();
	byte st = cio_open(ch, "D2:>SEEK.DAT", CIO_READ, 0);
	if (st >= 128)
	{
		con_printf("Open D2:>SEEK.DAT failed (%u).\n", st);
		return 1;
	}

	dword length = 0;
	byte last = 0;
	st = cio_file_length(ch, &length);
	if (st < 128 && length)
	{
		st = cio_file_seek(ch, -1, CIO_SEEK_END);
		if (st < 128)
			st = cio_getc(ch, &last);
	}
	byte closed = cio_close(ch);
	if (st < 128)
		st = closed;
	if (st >= 128)
	{
		con_printf("File position error (%u).\n", st);
		return 1;
	}
	if (length)
		con_printf("Length %lu, last byte %02X.\n", length, (word)last);
	else
		con_printf("Empty file.\n");
	return 0;
}
