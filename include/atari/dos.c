#include "dos.h"

void dos_exit(void)
{
	__asm {
		jmp	($000a)
	};
}

enum DOSLayout
{
	CRUNCH = 3,
	ARG_OFFSET = 10,
	CRUNCH_FLAGS = 26,
	CRUNCHED_NAME = 33,
	CRUNCHED_SIZE = 28,
	COMMAND_LINE = 63,
	LINE_SIZE = 64,
	ATASCII_EOL = 0x9b,
	GETCWD = 48
};

static volatile byte * sparta_table(void)
{
	const volatile byte * id = (const volatile byte *)0x0700;
	if (id[0] != 'S' || (id[1] & 0xf0) != 0x40)
		return 0;

	const volatile byte * vector = (const volatile byte *)0x000a;
	word address = vector[0] | (word)vector[1] << 8;
	if (address < 0x0700 || address > 0xc000 - COMMAND_LINE - LINE_SIZE)
		return 0;

	volatile byte * table = (volatile byte *)address;
	if (table[0] != 0x4c || table[CRUNCH] != 0x4c)
		return 0;
	return table;
}

byte dos_getcwd(byte drive, char * path, byte size)
{
	if (!sparta_table())
		return CIO_NOTIMPL;
	if (drive > 15)
		return CIO_INVFNM;
	if (!path || !size)
		return CIO_TRUNC;
	byte ch = cio_free();
	if (ch == 0xff)
		return CIO_TMOF;

	char name[4] = {'D', ':', 0, 0};
	if (drive)
	{
		name[1] = drive < 10 ? '0' + drive : 'A' - 1 + drive;
		name[2] = ':';
	}
	volatile char work[LINE_SIZE];
	iocb[ch].bll = (word)&work[0];
	byte status = cio_xio(GETCWD, ch, name, 0, 0);
	if (status >= 128)
		return status;

	byte length = 0;
	while (length < LINE_SIZE && work[length] && work[length] != ATASCII_EOL)
		length++;
	if (length >= LINE_SIZE || length >= size)
		return CIO_TRUNC;
	for (byte i = 0; i < length; i++)
		path[i] = work[i];
	path[length] = 0;
	return status;
}

byte dos_default_drive(void)
{
	volatile byte * table = sparta_table();
	if (!table)
		return 0;

	byte saved_name[CRUNCHED_SIZE];
	for (byte i = 0; i < CRUNCHED_SIZE; i++)
		saved_name[i] = table[CRUNCHED_NAME + i];
	byte saved_offset = table[ARG_OFFSET];
	byte saved_flags = table[CRUNCH_FLAGS];
	byte saved_first = table[COMMAND_LINE];
	byte saved_second = table[COMMAND_LINE + 1];
	table[ARG_OFFSET] = 0;
	table[COMMAND_LINE] = 'X';
	table[COMMAND_LINE + 1] = ATASCII_EOL;

	// Ask the resident crunch routine to add a device to a bare filename.
	((void (*)(void))(table + CRUNCH))();
	byte device = table[CRUNCHED_NAME];
	byte unit = table[CRUNCHED_NAME + 1];
	byte colon = table[CRUNCHED_NAME + 2];

	// SDX 4.49 also changes the parser flags while crunching the token.
	table[CRUNCH_FLAGS] = saved_flags;
	table[ARG_OFFSET] = saved_offset;
	table[COMMAND_LINE] = saved_first;
	table[COMMAND_LINE + 1] = saved_second;
	for (byte i = 0; i < CRUNCHED_SIZE; i++)
		table[CRUNCHED_NAME + i] = saved_name[i];

	if (device != 'D' || colon != ':')
		return 0;
	if (unit >= '1' && unit <= '9')
		return unit - '0';
	if (unit >= 'A' && unit <= 'O')
		return unit - 'A' + 1;
	return 0;
}

bool dos_args(DOSArgs * args)
{
	const volatile byte * table = sparta_table();
	if (!args || !table)
		return false;
	const volatile byte * line = table + COMMAND_LINE;
	byte length = 0;
	while (length < LINE_SIZE && line[length] != ATASCII_EOL)
		length++;
	if (length == LINE_SIZE)
		return false;

	byte argc = 0;
	bool word_start = true;
	for (byte i = 0; i < length; i++)
	{
		char c = line[i];
		if (c == ' ')
		{
			args->text[i] = 0;
			word_start = true;
		}
		else
		{
			args->text[i] = c;
			if (word_start)
				args->argv[argc++] = args->text + i;
			word_start = false;
		}
	}
	args->text[length] = 0;
	args->argv[argc] = (char *)0;
	args->argc = argc;
	return true;
}
