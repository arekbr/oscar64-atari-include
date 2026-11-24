#include "s2_cio.h"

static const char device[] = "S2:";

byte s2_cio_command(byte channel, byte command, byte aux1, byte aux2)
{
	return cio_xio(command, channel, device, aux1, aux2);
}

byte s2_cio_probe(byte channel)
{
	if (channel > 7)
		return CIO_BADIOCB;
	if (iocb[channel].hid != 255)
		return CIO_ISOPEN;
	iocb[channel].ax6 = 0;
	iocb[channel].bal = (word)device;
	iocb[channel].ax1 = iocb[channel].ax2 = 0;
	iocb[channel].com = 96;
	word response = cio_raw(channel);
	byte status = response >> 8;
	if (status >= 128)
		return status;
	return (byte)response == 96 || iocb[channel].ax6 == 96 ? CIO_OK : CIO_NOTIMPL;
}

byte s2_cio_open(byte channel, byte mode, byte flags, AS2Mode * result)
{
	if (mode > S2_TEXT_80X32)
		return CIO_BADCMD;
	byte status = s2_cio_probe(channel);
	if (status != CIO_OK)
		return status;
	status = cio_open(channel, device, CIO_UPDATE | (flags & (S2_NOCLEAR | S2_GRAYSCALE)), mode);
	if (status == CIO_OK && result)
	{
		result->width = iocb[channel].ax3 | (word)iocb[channel].ax4 << 8;
		result->height = iocb[channel].ax5;
		result->attributes = iocb[channel].ax6;
	}
	return status;
}

byte s2_cio_font_size(byte channel, byte * width, byte * height)
{
	if (channel > 7)
		return CIO_BADIOCB;
	byte saved = iocb[channel].ax1;
	byte status = s2_cio_command(channel, 114, 0, 0);
	iocb[channel].ax1 = saved;
	if (status == CIO_OK)
	{
		byte cell = iocb[channel].ax2;
		byte w = cell >> 4, h = cell & 15;
		if (width)
			*width = w && w != 15 ? w + 2 : 0;
		if (height)
			*height = h && h != 15 ? h + 2 : 0;
	}
	return status;
}
