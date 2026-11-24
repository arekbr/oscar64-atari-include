#include <atari/cio.h>
#include <atari/console.h>

int main(void)
{
	const char * oldname = "D:EXAMPLE.TMP";
	const char * newname = "D:SAVED.TMP";
	if (cio_exists(oldname) != 0 || cio_exists(newname) != 0)
	{
		con_printf("Use a writable DOS disk with no EXAMPLE.TMP or SAVED.TMP.\n");
		return 1;
	}
	byte ch = cio_free();
	byte status = cio_open(ch, oldname, CIO_WRITE, 0);
	if (status < 128)
	{
		status = cio_putc(ch, 'A');
		byte closed = cio_close(ch);
		if (status < 128)
			status = closed;
	}
	if (status < 128)
		status = cio_lock(oldname);
	if (status < 128)
		status = cio_unlock(oldname);
	if (status < 128)
		status = cio_rename(oldname, "SAVED.TMP");
	if (status < 128)
		status = cio_remove(newname);
	if (status >= 128)
	{
		con_printf("CIO status %u. Check EXAMPLE.TMP or SAVED.TMP.\n", status);
		return 1;
	}
	con_printf("Created, locked, unlocked, renamed and removed the file.\n");
	return 0;
}
