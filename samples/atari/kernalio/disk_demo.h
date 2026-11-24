#ifndef ATARI_SAMPLE_DISK_DEMO_H
#define ATARI_SAMPLE_DISK_DEMO_H

#include <atari/cio.h>
#include <atari/console.h>

// MyDOS uses a higher MEMLO than a bare XL/XE. Keep startup above DOS.
#pragma section(startup, 0)
#pragma region(startup, 0x2800, 0x2880, , , {startup})
#pragma region(main, 0x2880, 0x7800, , , {code, data, bss, heap, stack})

static int disk_result(byte status)
{
	if (status == CIO_OK)
		return 0;
	con_printf("CIO error %u\n", status);
	return 1;
}

static byte open_new(byte channel, const char * name)
{
	byte status = cio_open(channel, name, CIO_READ, 0);
	// MyDOS can leave the channel allocated even when OPEN returns an error.
	cio_close(channel);
	if (status == CIO_OK)
	{
		con_printf("File already exists. Use a scratch disk.\n");
		return CIO_LOCKED;
	}
	if (status != CIO_NOTFOUND)
		return status;
	return cio_open(channel, name, CIO_WRITE, 0);
}

#endif
