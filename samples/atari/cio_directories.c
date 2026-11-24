#include <atari/cio.h>
#include <atari/console.h>

#pragma section(startup, 0)
#pragma region(startup, 0x3000, 0x3080, , , {startup})
#pragma region(main, 0x3080, 0x8000, , , {code, data, bss, heap, stack})

#ifdef SPARTA
#define EXAMPLE_DOS CIO_DOS_SPARTA
#define EXAMPLE_ROOT "D2:>"
#else
#define EXAMPLE_DOS CIO_DOS_MYDOS
#define EXAMPLE_ROOT "D1:"
#endif

int main(void)
{
	const char * path = EXAMPLE_ROOT "EXAMPLE";
	byte st = cio_mkdir(path, EXAMPLE_DOS);
	if (st < 128)
		st = cio_chdir(path, EXAMPLE_DOS);
	if (st < 128)
		st = cio_chdir(EXAMPLE_ROOT, EXAMPLE_DOS);
	if (st < 128)
	{
#ifdef SPARTA
		st = cio_rmdir(path);
#else
		st = cio_remove(path);
#endif
	}
	if (st >= 128)
	{
		con_printf("CIO status %u. Check the disk and EXAMPLE directory.\n", st);
		return 1;
	}
	con_printf("Created, entered and removed EXAMPLE.\n");
	return 0;
}
