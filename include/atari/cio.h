#ifndef ATARI_CIO_H
#define ATARI_CIO_H

#include "types.h"

// Files and devices share the eight CIO channels (IOCBs 0 through 7).
// The OS opens the screen editor E: on channel 0. GRAPHICS can use channel 6
// for S:. D: requires a disk handler.
//
// CIO_OK indicates success. CIO_EOF_NEXT also indicates success.
// The next read reaches EOF after CIO_EOF_NEXT. CIO_EOF can accompany
// a short final read. Check the byte count before discarding the buffer.
// Use cio_failed() to test for errors.

#define CIO_OK			1
#define CIO_EOF_NEXT	3
#define CIO_BREAK		128		// BREAK pressed
#define CIO_ISOPEN		129		// channel already open
#define CIO_NODEV		130		// device not present
#define CIO_WRONLY		131		// write-only channel
#define CIO_BADCMD		132		// unknown command
#define CIO_NOTOPEN		133		// channel not open
#define CIO_BADIOCB		134		// invalid channel number
#define CIO_RDONLY		135		// read-only channel
#define CIO_EOF			136		// end of file
#define CIO_TRUNC		137		// record truncated (longer than buffer)
#define CIO_CURSOR		141		// screen cursor is out of range
#define CIO_NOTIMPL		146		// function not supported by handler
#define CIO_TMOF		161		// too many files or channels open
#define CIO_INVFNM		165		// invalid filename
#define CIO_PNTLEN		166		// POINT offset outside sector data
#define CIO_LOCKED		167		// file is locked
#define CIO_NOTFOUND	170		// file not found
#define CIO_PNTINV		171		// invalid POINT (meaning depends on the disk handler)

#define cio_failed(st)	((st) >= 128 && (st) != CIO_EOF)

// open mode (ICAX1)
#define CIO_READ		4
#define CIO_DIR			6		// read directory records
#define CIO_WRITE		8
#define CIO_APPEND		9		// append to an existing file
#define CIO_UPDATE		12		// read and write in place

// commands (ICCOM)
#define CIO_CMD_OPEN	0x03
#define CIO_CMD_GETREC	0x05
#define CIO_CMD_GETCHR	0x07
#define CIO_CMD_PUTREC	0x09
#define CIO_CMD_PUTCHR	0x0b
#define CIO_CMD_CLOSE	0x0c
#define CIO_CMD_STATUS	0x0d
#define CIO_CMD_POINT	0x25
#define CIO_CMD_NOTE	0x26
#define CIO_CMD_GETFL	0x27		// SpartaDOS X file length

// Disk handler XIO commands
#define CIO_XIO_RENAME	32		// name "D:OLD,NEW"
#define CIO_XIO_DELETE	33
#define CIO_XIO_LOCK	35
#define CIO_XIO_UNLOCK	36
#define CIO_XIO_CHDIR_MYDOS	41
#define CIO_XIO_MKDIR	42
#define CIO_XIO_RMDIR	43		// SpartaDOS
#define CIO_XIO_CHDIR_SPARTA	44

// Eight 16-byte IOCBs start at $0340.
struct IOCB
{
	byte	hid;		// ICHID  handler index, $FF = channel free
	byte	dno;		// ICDNO  drive number
	byte	com;		// ICCOM  command
	byte	sta;		// ICSTA  status of last operation
	word	bal;		// ICBAL/ICBAH  buffer address or name address
	word	ptl;		// ICPTL/ICPTH  PUT BYTE routine address minus one
	word	bll;		// ICBLL/ICBLH  buffer size, then number of bytes transferred
	byte	ax1;		// ICAX1  open mode
	byte	ax2;		// ICAX2
	byte	ax3, ax4, ax5, ax6;	// ICAX3 through ICAX6
};

#define iocb	((volatile struct IOCB *)0x0340)		// channels 0 through 7

// Call CIOV with the IOCB already filled in. The low byte of the result
// holds A and the high byte holds the status from Y.
// An invalid channel returns CIO_BADIOCB in the high byte.
// Treat the call as a memory barrier for buffers whose addresses are in
// an IOCB. The CIO wrappers below use the same barrier.
word cio_raw(byte ch);

// Open a channel. Pass the device name and optional filename in name,
// terminated by NUL or ATASCII EOL ($9B). CIO reads it from the caller's buffer.
byte cio_open(byte ch, const char * name, byte mode, byte aux2);

// Read or write bytes. The record commands use ATASCII EOL as a delimiter.
// Supply a non-null done pointer for the byte count reported by CIO.
// A zero length returns CIO_OK and sets *done to zero.
byte cio_read(byte ch, void * buf, word len, word * done);
byte cio_write(byte ch, const void * buf, word len, word * done);
byte cio_getrec(byte ch, void * buf, word len, word * done);
byte cio_putrec(byte ch, const void * buf, word len, word * done);

// Close a channel. Save any previous status before closing it.
byte cio_close(byte ch);

// Ask the handler for its current status. The command overwrites iocb[ch].sta.
byte cio_status(byte ch);

// Send a handler-specific command with a name and ICAX1/ICAX2.
// The wrapper leaves ICAX3 through ICAX6 as set by the caller.
byte cio_xio(byte cmd, byte ch, const char * name, byte aux1, byte aux2);

// The file position contains ICAX3 through ICAX5. Its layout and meaning
// depend on the handler. For DOS 2.x it holds a sector and a byte within
// that sector. Pass the value back unchanged to seek within the same file.
struct CIOPos
{
	word	sector;		// ICAX3/ICAX4
	byte	offset;		// ICAX5
};

// Store the current position in *pos. Leave *pos unchanged on error.
byte cio_note(byte ch, struct CIOPos * pos);

// Restore a position saved by cio_note(). POINT errors depend on the handler.
byte cio_point(byte ch, const struct CIOPos * pos);

#define CIO_SEEK_SET	0		// from the start
#define CIO_SEEK_CUR	1		// from the current position
#define CIO_SEEK_END	2		// from the end

// Byte offsets for an open D: file under SpartaDOS X 4.x.
// Other handlers or DOS versions return CIO_NOTIMPL. Closed channels
// return CIO_NOTOPEN.
// Store the offset or length in a non-null pointer. Leave it unchanged
// on error. A null pointer returns CIO_PNTINV.
byte cio_file_tell(byte ch, dword * offset);
byte cio_file_length(byte ch, dword * length);

// Apply a signed displacement from the selected origin. Invalid origins
// and positions outside 0 through $FFFFFF return CIO_PNTINV. The disk
// handler decides what happens when seeking beyond the end of the file.
byte cio_file_seek(byte ch, long offset, byte origin);

// Return the first unused channel from 1 through 7, or $FF if all are in use.
byte cio_free(void);

// Read or write one byte. The status follows cio_read() and cio_write().
// Use *c only when cio_getc() returns CIO_OK or CIO_EOF_NEXT.
byte cio_getc(byte ch, byte * c);
byte cio_putc(byte ch, byte c);

// Read and discard a record through its EOL. Treat CIO_TRUNC as CIO_OK
// because the one-byte buffer is only needed to consume the record.
byte cio_skiprec(byte ch);

// Try opening a device-qualified name such as "D:DATA.TXT" for reading.
// Return 1 if it opens, 0 on CIO_NOTFOUND, and the CIO status on other
// errors. A null or empty name returns CIO_INVFNM. With no free channel
// the result is CIO_TMOF. Wildcard matching depends on the handler.
byte cio_exists(const char * name);

// Change a file entry through its disk handler using a temporary free IOCB.
// Names must include the device, for example "D1:DATA.TXT", and end in
// NUL or ATASCII EOL. Return the CIO status without translating it.
// Wildcards are interpreted by the disk handler and can match several files.
byte cio_remove(const char * name);
byte cio_lock(const char * name);
byte cio_unlock(const char * name);

// Rename within the same directory. Give the device-qualified old name and
// the new leaf name, for example "D1:DATA.TXT" and "SAVED.TXT".
// The names can end in NUL or ATASCII EOL. Allow up to 126 bytes total
// across both names. Names containing a comma or space return CIO_INVFNM.
// Whether a destination may already exist depends on the disk handler.
byte cio_rename(const char * oldname, const char * newname);

#define CIO_DOS_MYDOS	0
#define CIO_DOS_SPARTA	1

// Create or select a directory on the named drive. Pass the dialect of
// the active disk handler. Paths end in NUL or ATASCII EOL.
// An unknown dialect returns CIO_NOTIMPL. The calls return the CIO status
// from a free IOCB, or CIO_TMOF when no channel is available.
byte cio_mkdir(const char * path, byte dos);
byte cio_chdir(const char * path, byte dos);

// Remove an empty directory with SpartaDOS. With MyDOS, use cio_remove()
// on a known empty directory. Atari DOS 2.x does not support subdirectories.
byte cio_rmdir(const char * path);

// Read DOS 2-style directory records with CIO_DIR and cio_getrec().
// A file entry has two flag columns, an eight-character name, a
// three-character extension and a decimal sector count. The first flag
// is '*' for a locked file and the second can be ':' for a subdirectory.
// A free-space entry ends in " FREE SECTORS". Other record layouts may
// return CIO_DE_NONE.
#define CIO_DE_NONE		0		// unrecognized record
#define CIO_DE_FILE		1		// file or subdirectory
#define CIO_DE_FREE		2		// free-space entry

struct CIODirEnt
{
	byte	kind;			// CIO_DE_*
	byte	locked;			// file is locked
	byte	isdir;			// entry is a subdirectory
	word	sectors;		// file size or free space in sectors, limited to 65535
	char	name[9];		// NUL-terminated name without padding
	char	ext[4];			// NUL-terminated extension without padding
};

// Parse len bytes of a directory record, up to the first ATASCII EOL.
// Return the resulting e->kind.
byte cio_dirparse(const char * rec, word len, struct CIODirEnt * e);

// Read and parse a directory record with a 24-byte buffer. Return the
// CIO status. On CIO_TRUNC, parse the available prefix. End of file and
// other errors leave e->kind set to CIO_DE_NONE.
byte cio_dirread(byte ch, struct CIODirEnt * e);

#pragma compile("cio.c")

#endif
