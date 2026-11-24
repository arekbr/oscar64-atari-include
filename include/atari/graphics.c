#include "graphics.h"
#include "os.h"

#define S_DRAW		0x11
#define S_CLEAR		0x7d
#define os_atachr	(*(volatile byte *)0x02fb)

static const AGraphicsMode modes[] = {
	{ 40, 24, 0, 0 },	// 0
	{ 20, 24, 20, 0 },
	{ 20, 12, 10, 0 },
	{ 40, 24, 20, 2 },
	{ 80, 48, 40, 1 },
	{ 80, 48, 40, 2 },	// 5
	{ 160, 96, 80, 1 },
	{ 160, 96, 80, 2 },
	{ 320, 192, 160, 1 },
	{ 80, 192, 0, 4 },
	{ 80, 192, 0, 4 },	// 10
	{ 80, 192, 0, 4 },
	{ 40, 24, 20, 0 },
	{ 40, 12, 10, 0 },
	{ 160, 192, 160, 1 },
	{ 160, 192, 160, 2 }	// 15
};

const AGraphicsMode * gr_mode_info(byte mode)
{
	if (mode < 16)
		return modes + mode;
	return 0;
}

byte gr_open(AGraphics * screen, byte channel, byte mode, byte flags)
{
	if (!channel || channel > 7)
		return CIO_BADIOCB;
	const AGraphicsMode * info = gr_mode_info(mode);
	if (!screen || !info || (flags & ~(GR_SPLIT | GR_NOCLEAR)))
		return CIO_BADCMD;

	byte height = flags & GR_SPLIT ? info->split_height : info->height;
	if (!height)
		return CIO_BADCMD;
	if (iocb[channel].hid != 0xff)
		return CIO_ISOPEN;

	byte status = cio_open(channel, "S:", CIO_UPDATE | flags, mode);
	if (status != CIO_OK)
		return status;
	if (os_dindex != mode)
	{
		cio_close(channel);
		return CIO_BADCMD;
	}

	screen->width = info->width;
	screen->height = height;
	screen->mode = mode;
	screen->channel = channel;
	screen->handler = iocb[channel].hid;
	screen->bits = info->bits;
	return status;
}

static bool gr_channel_open(const AGraphics * screen)
{
	return screen && screen->channel && screen->channel <= 7 &&
		screen->handler != 0xff && iocb[screen->channel].hid == screen->handler;
}

byte gr_close(AGraphics * screen)
{
	if (!gr_channel_open(screen))
		return CIO_NOTOPEN;
	byte status = cio_close(screen->channel);
	if (status == CIO_OK)
		screen->channel = 0;
	return status;
}

static byte gr_position(const AGraphics * screen, word x, byte y)
{
	if (!gr_channel_open(screen) || os_dindex != screen->mode)
		return CIO_NOTOPEN;
	if (x >= screen->width || y >= screen->height)
		return CIO_CURSOR;
	os_colcrs = x;
	os_rowcrs = y;
	return CIO_OK;
}

byte gr_plot(const AGraphics * screen, word x, byte y, byte value)
{
	byte status = gr_position(screen, x, y);
	if (status != CIO_OK)
		return status;
	if (screen->bits)
		value &= (1 << screen->bits) - 1;
	return cio_putc(screen->channel, value);
}

byte gr_locate(const AGraphics * screen, word x, byte y, byte * value)
{
	if (!value)
		return CIO_BADCMD;
	byte status = gr_position(screen, x, y);
	if (status != CIO_OK)
		return status;
	byte pixel;
	status = cio_getc(screen->channel, &pixel);
	if (status == CIO_OK)
		*value = pixel;
	return status;
}

byte gr_draw_to(const AGraphics * screen, word x, byte y, byte color)
{
	if (screen && !screen->bits)
		return CIO_BADCMD;
	byte status = gr_position(screen, x, y);
	if (status != CIO_OK)
		return status;
	os_atachr = color & ((1 << screen->bits) - 1);
	byte channel = screen->channel;
	iocb[channel].com = S_DRAW;
	return cio_raw(channel) >> 8;
}

byte gr_clear(const AGraphics * screen)
{
	byte status = gr_position(screen, 0, 0);
	if (status != CIO_OK)
		return status;
	byte display = os_dspflg;
	os_dspflg = 0;
	status = cio_putc(screen->channel, S_CLEAR);
	os_dspflg = display;
	return status;
}
