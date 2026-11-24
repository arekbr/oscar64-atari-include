#include <atari/console.h>
#include <atari/dos.h>
#include <atari/input.h>

// SpartaDOS X may keep resident code below $3000.
#pragma section(startup, 0)
#pragma region(startup, 0x3000, 0x3080, , , {startup})
#pragma region(main, 0x3080, 0x7000, , , {code, data, bss, heap, stack})

static DOSArgs args;
static char directory[64];

static void show_context(void)
{
	byte drive = dos_default_drive();
	if (drive)
	{
		char unit = drive < 10 ? '0' + drive : 'A' - 1 + drive;
		con_printf("Default drive: D%c:\n", unit);
	}
	else
		con_printf("Default drive: unavailable\n");

	byte status = dos_getcwd(0, directory, sizeof(directory));
	if (status < 128)
		con_printf("Current directory: %s\n", directory[0] ? directory : "\\");
	else
		con_printf("Current directory error: %u\n", (word)status);

	con_printf("Program: %s\n", args.argc ? args.argv[0] : "(unavailable)");
	con_printf("Arguments:");
	if (args.argc <= 1)
		con_printf(" (none)");
	for (byte i = 1; i < args.argc; i++)
		con_printf(" %s", args.argv[i]);
	con_printf("\n");
}

int main(void)
{
	// Save the launch line before making other DOS queries.
	bool ready = dos_args(&args);
	con_clrscr();
	if (!ready)
	{
		con_printf("Run this example from SpartaDOS X\n");
		return 1;
	}

	con_printf("SpartaDOS X context\n");
	show_context();
	con_printf("Press a key to return\n");
	key_getc();
	dos_exit();
	return 0;
}
