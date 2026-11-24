#include "pmg.h"
#include "antic.h"
#include "os.h"

static volatile byte * pmg_base;
static byte pmg_single;

struct PMGSpan
{
	word count;
	byte y;
};

static PMGSpan spans[8];

void pmg_clear(void)
{
	if (!pmg_base)
		return;
	volatile byte * p = pmg_base + (pmg_single ? 0x300 : 0x180);
	word n = pmg_single ? 0x500 : 0x280;
	while (n--)
		*p++ = 0;
	for (byte i = 0; i < 8; i++)
		spans[i].count = 0;
}

void pmg_init(volatile byte * base, byte resolution)
{
	pmg_base = base;
	pmg_single = resolution;

	pmg_clear();

	antic.pmbase = (byte)((word)base >> 8);
	os_sdmctl = (os_sdmctl & (byte)~DMACTL_PM_1LINE) | DMACTL_PLAYERS | DMACTL_MISSILES
				| (resolution ? DMACTL_PM_1LINE : 0);
	gtia.gractl = GRACTL_PLAYERS | GRACTL_MISSILES;
}

void pmg_off(void)
{
	gtia.gractl = 0;
	os_sdmctl &= (byte)~(DMACTL_PLAYERS | DMACTL_MISSILES | DMACTL_PM_1LINE);
	for (byte i = 0; i < 4; i++)
	{
		gtia.hposp[i] = 0;
		gtia.hposm[i] = 0;
		gtia.grafp[i] = 0;
	}
	gtia.grafm = 0;
}

volatile byte * pmg_player(byte n)
{
	return pmg_single ? pmg_base + 0x400 + ((word)n << 8) : pmg_base + 0x200 + ((word)n << 7);
}

word pmg_plane_rows(void)
{
	return pmg_base ? (pmg_single ? 256 : 128) : 0;
}

volatile byte * pmg_missiles(void)
{
	return pmg_base + (pmg_single ? 0x300 : 0x180);
}

byte pmg_missile_size(byte missile, byte size)
{
	size &= 3;
	return missile < 4 && size != 2 ? size << (missile << 1) : 0;
}

struct PMGPlane
{
	volatile byte * data, * position;
	PMGSpan * last;
	byte shift, mask;
};

static bool place(const PMGPlane * plane, const PMGShape * shape)
{
	if (!shape)
		return false;
	word limit = pmg_single ? 256 : 128;
	const byte * pixels = shape->scanlines;
	word count = shape->count;
	byte x = shape->x, y = shape->y;
	if (y >= limit)
		count = 0;
	else if (count > limit - y)
		count = limit - y;
	if (count)
	{
		word source = (word)pixels;
		word start = (word)pmg_base + (pmg_single ? 0x300 : 0x180);
		word end = (word)pmg_base + (pmg_single ? 0x7ff : 0x3ff);
		if (!source || source > 0xffffu - (count - 1) ||
			(source <= end && start <= source + (count - 1)))
			return false;
	}
	volatile byte * row = plane->data + plane->last->y;
	byte preserve = ~plane->mask;
	for (word i = 0; i < plane->last->count; i++)
		row[i] &= preserve;
	row = plane->data + y;
	for (word i = 0; i < count; i++)
	{
		byte value = (pixels[i] << plane->shift) & plane->mask;
		row[i] = (row[i] & preserve) | value;
	}
	plane->last->y = y;
	plane->last->count = count;
	*plane->position = count ? x : 0;
	return true;
}

bool pmg_move_player(byte player, const PMGShape * shape)
{
	if (player >= 4 || !pmg_base)
		return false;
	PMGPlane plane = {pmg_player(player), &gtia.hposp[player], spans + player, 0, 0xff};
	return place(&plane, shape);
}

bool pmg_move_missile(byte missile, const PMGShape * shape)
{
	if (missile >= 4 || !pmg_base)
		return false;
	byte shift = missile << 1;
	PMGPlane plane = {pmg_missiles(), &gtia.hposm[missile], spans + 4 + missile, shift, 3 << shift};
	return place(&plane, shape);
}
