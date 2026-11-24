#ifndef ATARI_SIO_H
#define ATARI_SIO_H

#include "types.h"

// Serial I/O uses the OS Device Control Block (DCB) at $0300 and SIOV at $E459.
// Keep the OS ROM mapped and its interrupts active for serial transfers and timeouts.
// Leave the OS and any resident DOS in control of their RAM work areas.
//
// SIO calls return DSTATS from the DCB. SIO_OK (1) means success, and values
// of 128 or higher indicate an error.

#define SIO_OK			1
#define SIO_TIMEOUT		138		// No response from the device
#define SIO_NAK			139
#define SIO_FRAMING		140
#define SIO_OVERRUN		142
#define SIO_CHECKSUM	143
#define SIO_DEVDONE		144		// The device reported an operation error

#define SIO_DEV_DISK	0x31	// Use this DDEVIC for disks and select the drive with DUNIT (1 through 8).

// DSTATS transfer direction on entry.
#define SIO_NONE		0x00
#define SIO_READ		0x40	// Device to computer
#define SIO_WRITE		0x80	// Computer to device

// Disk drive commands.
#define SIO_CMD_PUT		0x50	// 'P' writes a sector without verification
#define SIO_CMD_READ	0x52	// 'R' reads a sector
#define SIO_CMD_STATUS	0x53	// 'S' reads four drive status bytes
#define SIO_CMD_WRITE	0x57	// 'W' writes and verifies a sector

// The 12-byte Device Control Block at $0300.
struct SIODCB
{
	byte	ddevic;		// DDEVIC  Base device ID
	byte	dunit;		// DUNIT   Unit number (1-based)
	byte	dcomnd;		// DCOMND  Command
	byte	dstats;		// DSTATS  Transfer direction on entry, status on return
	word	dbuf;		// DBUFLO/DBUFHI  Buffer address
	byte	dtimlo;		// DTIMLO  Timeout in seconds
	byte	dunuse;		// DUNUSE
	word	dbyt;		// DBYTLO/DBYTHI  Byte count
	byte	daux1;		// DAUX1  Command-specific byte, typically sector low byte
	byte	daux2;		// DAUX2  Command-specific byte, typically sector high byte
};

#define sio_dcb		((volatile struct SIODCB *)0x0300)

// Call SIOV with the current DCB and return its updated DSTATS.
// A compiler memory barrier covers address-taken buffers visible to Oscar64.
// When filling DBUF directly, calculate offsets on the pointer before casting
// to word, for example (word)(buffer + offset).
byte sio_call(void);

// Fill the DCB and call SIOV with a 15-second timeout. Pass the direction
// bits in dir and pack DAUX1 into the low byte of aux, DAUX2 into the high byte.
byte sio_xfer(byte dev, byte unit, byte cmd, byte dir, void * buf, word len, word aux);

// Request a 128-byte transfer. Drive and sector numbers are 1-based.
// On conventional double-density disks, sectors 1 through 3 are 128 bytes and
// subsequent sectors are 256 bytes. Use these helpers only for 128-byte
// sectors and provide a buffer of at least 128 bytes.
byte sio_read_sector(byte unit, word sector, void * buf);
byte sio_write_sector(byte unit, word sector, const void * buf);		// 'W' verifies the write
byte sio_put_sector(byte unit, word sector, const void * buf);			// 'P' skips verification

// Four bytes returned by the disk drive status command.
struct SIODrive
{
	byte	drvst;		// Drive flags, including write protection and density
	byte	fdcst;		// Floppy disk controller status
	byte	timeout;	// Format timeout
	byte	unused;
};
#define SIO_DRV_WP		0x08	// Write-protected media
#define SIO_DRV_DD		0x20	// Double density (256 bytes per sector from sector 4)

// Use the returned drive status bytes only after SIO_OK.
byte sio_drive_status(byte unit, struct SIODrive * st);

// Report the conventional disk sector size for sectors 4 and up.
// This uses the drive's density flag and does not check the sector number.
#define sio_sector_size(st)		(((st)->drvst & SIO_DRV_DD) ? 256 : 128)

// A disk helper rejected its parameters before issuing an SIO command.
#define SIO_BADARG		0

// Query one sector using a successful drive status request. The first three
// sectors are 128 bytes even on conventional double-density disks.
// Return zero for a missing status or sector number zero.
word sio_disk_sector_size(const struct SIODrive * st, word sector);

// Transfer one sector using the supplied drive status. Refresh that status
// after a media change. size is the number of bytes available in buf.
// An invalid drive, sector zero, or a short or wrapping buffer returns
// SIO_BADARG. Otherwise return the OS SIO status for the requested sector.
byte sio_disk_read(byte unit, word sector, void * buf, word size, const struct SIODrive * st);
byte sio_disk_write(byte unit, word sector, const void * buf, word size, const struct SIODrive * st);
byte sio_disk_put(byte unit, word sector, const void * buf, word size, const struct SIODrive * st);

#pragma compile("sio.c")

#endif
