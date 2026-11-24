#include <atari/console.h>
#include <atari/sio.h>

#define DRIVE 1
#define SECTOR 720

static void show_sector(void)
{
	static struct SIODrive drive;
	static byte data[128];

	byte status = sio_drive_status(DRIVE, &drive);
	if (status != SIO_OK)
	{
		con_printf("D1: status failed (%u)\n", (unsigned)status);
		return;
	}

	word size = sio_sector_size(&drive);
	con_printf("D1: %u-byte sectors (4 and up)\n", size);
	if (size != sizeof(data))
	{
		con_printf("Sector %u needs a larger buffer\n", (unsigned)SECTOR);
		return;
	}

	status = sio_read_sector(DRIVE, SECTOR, data);
	if (status != SIO_OK)
	{
		con_printf("Sector %u read failed (%u)\n", (unsigned)SECTOR, (unsigned)status);
		return;
	}

	con_printf("Sector %u, first 16 bytes:\n", (unsigned)SECTOR);
	for (byte i = 0; i < 16; ++i)
		con_printf("%02X%c", (unsigned)data[i], (i & 7) == 7 ? '\n' : ' ');
}

int main(void)
{
	con_clrscr();
	show_sector();
	return 0;
}
