#include "sio.h"

// Read the result from DSTATS. Oscar64 may treat Y as preserved across
// the inline assembly call even though SIOV returns the status in Y.
// Access memory through a volatile pointer of unknown value before SIOV.
// This keeps the optimizer from moving buffer writes past the call or
// reusing cached values for address-taken buffers after SIOV.
static byte sio_dummy;
static volatile byte * volatile sio_mb = &sio_dummy;

byte sio_call(void)
{
	volatile byte * p = sio_mb;
	*p = *p;
	__asm {
		jsr	0xe459
	};
	return sio_dcb->dstats;
}

// Set up the DCB in each wrapper to keep Oscar64 argument frames small.
#define SIO_SETUP(dev, unit, cmd, dir, buf, len, aux)	\
	sio_dcb->ddevic = dev;								\
	sio_dcb->dunit = unit;								\
	sio_dcb->dcomnd = cmd;								\
	sio_dcb->dstats = dir;								\
	sio_dcb->dbuf = (word)(buf);						\
	sio_dcb->dtimlo = 15;								\
	sio_dcb->dunuse = 0;								\
	sio_dcb->dbyt = len;								\
	sio_dcb->daux1 = (aux) & 0xff;						\
	sio_dcb->daux2 = (aux) >> 8

byte sio_xfer(byte dev, byte unit, byte cmd, byte dir, void * buf, word len, word aux)
{
	SIO_SETUP(dev, unit, cmd, dir, buf, len, aux);
	return sio_call();
}

byte sio_read_sector(byte unit, word sector, void * buf)
{
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_READ, SIO_READ, buf, 128, sector);
	return sio_call();
}

byte sio_write_sector(byte unit, word sector, const void * buf)
{
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_WRITE, SIO_WRITE, buf, 128, sector);
	return sio_call();
}

byte sio_put_sector(byte unit, word sector, const void * buf)
{
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_PUT, SIO_WRITE, buf, 128, sector);
	return sio_call();
}

byte sio_drive_status(byte unit, struct SIODrive * st)
{
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_STATUS, SIO_READ, st, 4, 4);
	return sio_call();
}

word sio_disk_sector_size(const struct SIODrive * st, word sector)
{
	if (!st || !sector)
		return 0;
	return sector < 4 ? 128 : sio_sector_size(st);
}

static word disk_transfer_size(byte unit, word sector, const void * buf, word size, const struct SIODrive * st)
{
	word len = sio_disk_sector_size(st, sector);
	if (!unit || unit > 8 || !buf || !len || size < len || (word)buf > 0xffffu - (len - 1))
		return 0;
	return len;
}

byte sio_disk_read(byte unit, word sector, void * buf, word size, const struct SIODrive * st)
{
	word len = disk_transfer_size(unit, sector, buf, size, st);
	if (!len)
		return SIO_BADARG;
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_READ, SIO_READ, buf, len, sector);
	return sio_call();
}

byte sio_disk_write(byte unit, word sector, const void * buf, word size, const struct SIODrive * st)
{
	word len = disk_transfer_size(unit, sector, buf, size, st);
	if (!len)
		return SIO_BADARG;
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_WRITE, SIO_WRITE, buf, len, sector);
	return sio_call();
}

byte sio_disk_put(byte unit, word sector, const void * buf, word size, const struct SIODrive * st)
{
	word len = disk_transfer_size(unit, sector, buf, size, st);
	if (!len)
		return SIO_BADARG;
	SIO_SETUP(SIO_DEV_DISK, unit, SIO_CMD_PUT, SIO_WRITE, buf, len, sector);
	return sio_call();
}
