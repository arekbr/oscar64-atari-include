#include "cio.h"

// CIOV expects the IOCB offset in X. Keep the value in volatile storage
// because only the inline assembly reads it.
static volatile byte cio_x;

// CIO receives buffer addresses as words in the IOCB. The compiler cannot
// see that the OS can read or write through them. A read and write through
// a volatile pointer to a dummy byte makes the compiler preserve writes
// before CIOV and reload memory afterward.
static byte cio_dummy;
static volatile byte * volatile cio_mb = &cio_dummy;

word cio_raw(byte ch)
{
	if (ch > 7)
		return (word)CIO_BADIOCB << 8;
	cio_x = ch << 4;
	volatile byte * p = cio_mb;
	*p = *p;
	return __asm {
		ldx	cio_x
		jsr	0xe456
		sta	accu
		sty	accu + 1
	};
}

// Fill the IOCB in each wrapper. The transfer functions call cio_raw
// directly to avoid a compiler assertion with nested calls on Atari.

static byte cio_cmd(byte ch, byte cmd)
{
	iocb[ch].com = cmd;
	return cio_raw(ch) >> 8;
}

byte cio_open(byte ch, const char * name, byte mode, byte aux2)
{
	if (ch > 7)
		return CIO_BADIOCB;
	iocb[ch].bal = (word)name;
	iocb[ch].ax1 = mode;
	iocb[ch].ax2 = aux2;
	return cio_cmd(ch, CIO_CMD_OPEN);
}

byte cio_xio(byte cmd, byte ch, const char * name, byte aux1, byte aux2)
{
	if (ch > 7)
		return CIO_BADIOCB;
	iocb[ch].bal = (word)name;
	iocb[ch].ax1 = aux1;
	iocb[ch].ax2 = aux2;
	return cio_cmd(ch, cmd);
}

#define CIO_XFER(cmd)						\
	*done = 0;								\
	if (ch > 7)								\
		return CIO_BADIOCB;					\
	if (len == 0)							\
		return CIO_OK;						\
	iocb[ch].bal = (word)buf;				\
	iocb[ch].bll = len;						\
	iocb[ch].com = cmd;						\
	byte st = cio_raw(ch) >> 8;				\
	*done = iocb[ch].bll;					\
	return st

byte cio_read(byte ch, void * buf, word len, word * done)
{
	CIO_XFER(CIO_CMD_GETCHR);
}

byte cio_write(byte ch, const void * buf, word len, word * done)
{
	CIO_XFER(CIO_CMD_PUTCHR);
}

byte cio_getrec(byte ch, void * buf, word len, word * done)
{
	CIO_XFER(CIO_CMD_GETREC);
}

byte cio_putrec(byte ch, const void * buf, word len, word * done)
{
	CIO_XFER(CIO_CMD_PUTREC);
}

byte cio_close(byte ch)
{
	if (ch > 7)
		return CIO_BADIOCB;
	return cio_cmd(ch, CIO_CMD_CLOSE);
}

byte cio_status(byte ch)
{
	if (ch > 7)
		return CIO_BADIOCB;
	return cio_cmd(ch, CIO_CMD_STATUS);
}

byte cio_note(byte ch, struct CIOPos * pos)
{
	if (ch > 7)
		return CIO_BADIOCB;
	byte st = cio_cmd(ch, CIO_CMD_NOTE);
	if (st < 128)
	{
		pos->sector = iocb[ch].ax3 | (iocb[ch].ax4 << 8);
		pos->offset = iocb[ch].ax5;
	}
	return st;
}

byte cio_point(byte ch, const struct CIOPos * pos)
{
	if (ch > 7)
		return CIO_BADIOCB;
	iocb[ch].ax3 = pos->sector & 0xff;
	iocb[ch].ax4 = pos->sector >> 8;
	iocb[ch].ax5 = pos->offset;
	return cio_cmd(ch, CIO_CMD_POINT);
}

static byte cio_file_channel(byte ch)
{
	if (ch > 7)
		return CIO_BADIOCB;
	byte hid = iocb[ch].hid;
	if (hid == 0xff)
		return CIO_NOTOPEN;

	// Check the DOS type before issuing GETFL. Command 39 loads a
	// binary file under MyDOS.
	const volatile byte * dos = (const volatile byte *)0x0700;
	const volatile byte * handlers = (const volatile byte *)0x031a;
	if (dos[0] != 'S' || (dos[1] & 0xf0) != 0x40 || hid >= 33 || handlers[hid] != 'D')
		return CIO_NOTIMPL;
	return CIO_OK;
}

static byte cio_bytepos(byte ch, byte cmd, dword * value)
{
	byte st = cio_cmd(ch, cmd);
	if (st < 128)
		*value = (word)iocb[ch].ax3 | ((word)iocb[ch].ax4 << 8) | ((dword)iocb[ch].ax5 << 16);
	return st;
}

byte cio_file_tell(byte ch, dword * offset)
{
	byte st = cio_file_channel(ch);
	if (st >= 128)
		return st;
	if (!offset)
		return CIO_PNTINV;
	return cio_bytepos(ch, CIO_CMD_NOTE, offset);
}

byte cio_file_length(byte ch, dword * length)
{
	byte st = cio_file_channel(ch);
	if (st >= 128)
		return st;
	if (!length)
		return CIO_PNTINV;
	return cio_bytepos(ch, CIO_CMD_GETFL, length);
}

byte cio_file_seek(byte ch, long offset, byte origin)
{
	byte st = cio_file_channel(ch);
	if (st >= 128)
		return st;
	dword base = 0;
	switch (origin)
	{
	case CIO_SEEK_SET:
		break;
	case CIO_SEEK_CUR:
		st = cio_bytepos(ch, CIO_CMD_NOTE, &base);
		break;
	case CIO_SEEK_END:
		st = cio_bytepos(ch, CIO_CMD_GETFL, &base);
		break;
	default:
		return CIO_PNTINV;
	}
	if (st >= 128)
		return st;

	// A negative result wraps above the 24-bit limit. Use unsigned
	// addition so a large positive displacement cannot overflow long.
	dword target = base + (dword)offset;
	if (target > 0xffffffUL)
		return CIO_PNTINV;
	struct CIOPos pos;
	pos.sector = (word)target;
	pos.offset = (byte)(target >> 16);
	return cio_point(ch, &pos);
}

byte cio_free(void)
{
	for (byte ch = 1; ch < 8; ch++)
		if (iocb[ch].hid == 0xff)
			return ch;
	return 0xff;
}

// CIO uses these single-byte buffers directly.
static volatile byte cio_pb;		// byte for cio_putc
static volatile byte cio_sk;		// 1-byte buffer for cio_skiprec

byte cio_getc(byte ch, byte * c)
{
	if (ch > 7)
		return CIO_BADIOCB;
	iocb[ch].bal = (word)c;
	iocb[ch].bll = 1;
	iocb[ch].com = CIO_CMD_GETCHR;
	return cio_raw(ch) >> 8;
}

byte cio_putc(byte ch, byte c)
{
	if (ch > 7)
		return CIO_BADIOCB;
	cio_pb = c;
	iocb[ch].bal = (word)&cio_pb;
	iocb[ch].bll = 1;
	iocb[ch].com = CIO_CMD_PUTCHR;
	return cio_raw(ch) >> 8;
}

byte cio_skiprec(byte ch)
{
	if (ch > 7)
		return CIO_BADIOCB;
	iocb[ch].bal = (word)&cio_sk;
	iocb[ch].bll = 1;
	iocb[ch].com = CIO_CMD_GETREC;
	byte st = cio_raw(ch) >> 8;
	return st == CIO_TRUNC ? CIO_OK : st;
}

byte cio_exists(const char * name)
{
	if (name == 0 || name[0] == 0)
		return CIO_INVFNM;
	byte ch = cio_free();
	if (ch == 0xff)
		return CIO_TMOF;
	iocb[ch].bal = (word)name;
	iocb[ch].ax1 = CIO_READ;
	iocb[ch].ax2 = 0;
	byte st = cio_cmd(ch, CIO_CMD_OPEN);
	// Some handlers leave the channel allocated when OPEN fails.
	cio_cmd(ch, CIO_CMD_CLOSE);
	if (st < 128)
		return 1;
	return st == CIO_NOTFOUND ? 0 : st;
}

static byte cio_file_cmd(byte cmd, const char * name, byte aux1)
{
	if (!name || !name[0] || (byte)name[0] == 0x9b)
		return CIO_INVFNM;
	byte ch = cio_free();
	if (ch == 0xff)
		return CIO_TMOF;
	iocb[ch].bal = (word)name;
	iocb[ch].bll = 0;
	iocb[ch].ax1 = aux1;
	iocb[ch].ax2 = 0;
	return cio_cmd(ch, cmd);
}

byte cio_remove(const char * name)
{
	return cio_file_cmd(CIO_XIO_DELETE, name, 0);
}

byte cio_lock(const char * name)
{
	return cio_file_cmd(CIO_XIO_LOCK, name, 0);
}

byte cio_unlock(const char * name)
{
	return cio_file_cmd(CIO_XIO_UNLOCK, name, 0);
}

// DOS expects both names in the same CIO buffer. Stop at the terminator
// and leave room for a separator and EOL.
static byte cio_rename_copy(char * dst, const char * src, byte maxlen)
{
	byte len = 0;
	while (src[len] && (byte)src[len] != 0x9b)
	{
		if (len == maxlen || src[len] == ',' || src[len] == ' ')
			return 0;
		dst[len] = src[len];
		len++;
	}
	return len;
}

byte cio_rename(const char * oldname, const char * newname)
{
	if (!oldname || !newname)
		return CIO_INVFNM;
	char names[128];
	byte oldlen = cio_rename_copy(names, oldname, sizeof(names) - 3);
	if (!oldlen)
		return CIO_INVFNM;
	byte newlen = cio_rename_copy(names + oldlen + 1, newname, sizeof(names) - oldlen - 2);
	if (!newlen)
		return CIO_INVFNM;
	names[oldlen] = ',';
	names[oldlen + 1 + newlen] = (char)0x9b;
	return cio_file_cmd(CIO_XIO_RENAME, names, 0);
}

byte cio_mkdir(const char * path, byte dos)
{
	if (dos != CIO_DOS_MYDOS && dos != CIO_DOS_SPARTA)
		return CIO_NOTIMPL;
	return cio_file_cmd(CIO_XIO_MKDIR, path, dos == CIO_DOS_MYDOS ? CIO_WRITE : 0);
}

byte cio_chdir(const char * path, byte dos)
{
	if (dos == CIO_DOS_MYDOS)
		return cio_file_cmd(CIO_XIO_CHDIR_MYDOS, path, 0);
	if (dos == CIO_DOS_SPARTA)
		return cio_file_cmd(CIO_XIO_CHDIR_SPARTA, path, 0);
	return CIO_NOTIMPL;
}

byte cio_rmdir(const char * path)
{
	return cio_file_cmd(CIO_XIO_RMDIR, path, 0);
}

static const char dir_free[] = " FREE SECTORS";

byte cio_dirparse(const char * rec, word len, struct CIODirEnt * e)
{
	e->kind = CIO_DE_NONE;
	e->locked = 0;
	e->isdir = 0;
	e->sectors = 0;
	e->name[0] = 0;
	e->ext[0] = 0;

	byte n = 0;		// record length, excluding the ATASCII EOL
	while (n < len && n < 255 && rec[n] != 0x9b)
		n++;

	byte i = 0;
	while (i < n && rec[i] == ' ')
		i++;
	byte d0 = i;
	word v = 0;
	while (i < n && rec[i] >= '0' && rec[i] <= '9')
	{
		if (v < 6553 || (v == 6553 && rec[i] <= '5'))		// saturate at 65535
			v = v * 10 + (rec[i] - '0');
		else
			v = 65535;
		i++;
	}
	if (i > d0 && i - d0 <= 5 && n - i == 13)
	{
		byte k = 0;
		while (k < 13 && rec[i + k] == dir_free[k])
			k++;
		if (k == 13)
		{
			e->sectors = v;
			e->kind = CIO_DE_FREE;
			return CIO_DE_FREE;
		}
	}

	// The sector count starts at or after column 13 (zero-based).
	if (n < 15 || (rec[0] != ' ' && rec[0] != '*') || (rec[1] != ' ' && rec[1] != ':'))
		return CIO_DE_NONE;
	i = 13;
	while (i < n && rec[i] == ' ')
		i++;
	d0 = i;
	v = 0;
	while (i < n && rec[i] >= '0' && rec[i] <= '9')
	{
		if (v < 6553 || (v == 6553 && rec[i] <= '5'))		// saturate at 65535
			v = v * 10 + (rec[i] - '0');
		else
			v = 65535;
		i++;
	}
	if (i == d0 || i != n)
		return CIO_DE_NONE;

	e->locked = rec[0] == '*';
	e->isdir = rec[1] == ':';
	e->sectors = v;
	byte m = 8;
	while (m > 0 && rec[1 + m] == ' ')
		m--;
	for (byte j = 0; j < m; j++)
		e->name[j] = rec[2 + j];
	e->name[m] = 0;
	m = 3;
	while (m > 0 && rec[9 + m] == ' ')
		m--;
	for (byte j = 0; j < m; j++)
		e->ext[j] = rec[10 + j];
	e->ext[m] = 0;
	e->kind = CIO_DE_FILE;
	return CIO_DE_FILE;
}

static char dir_buf[24];

byte cio_dirread(byte ch, struct CIODirEnt * e)
{
	if (ch > 7)
	{
		e->kind = CIO_DE_NONE;
		return CIO_BADIOCB;
	}
	// Call cio_raw directly, as in CIO_XFER.
	iocb[ch].bal = (word)dir_buf;
	iocb[ch].bll = sizeof(dir_buf);
	iocb[ch].com = CIO_CMD_GETREC;
	byte st = cio_raw(ch) >> 8;
	if (st < 128 || st == CIO_TRUNC)
		cio_dirparse(dir_buf, iocb[ch].bll, e);
	else
		e->kind = CIO_DE_NONE;
	return st;
}
