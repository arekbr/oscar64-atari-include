#ifndef ATARI_POINTER_DECODE_H
#define ATARI_POINTER_DECODE_H

#include "pointer.h"

static __forceinline sbyte pointer_quadrature(byte old, byte next)
{
	static const byte phase[] = {0, 1, 3, 2};
	byte step = (phase[next] - phase[old]) & 3;
	return step == 1 ? 1 : step == 3 ? -1 : 0;
}

static __forceinline void pointer_decode(byte type, byte old, byte now, sbyte * dx, sbyte * dy)
{
	*dx = *dy = 0;
	switch (type)
	{
	case POINTER_ST:
		*dx = pointer_quadrature(now & 3, old & 3);
		*dy = pointer_quadrature(now >> 2, old >> 2);
		break;
	case POINTER_AMIGA:
		*dx = pointer_quadrature(((old >> 1) & 1) | ((old >> 2) & 2),
			((now >> 1) & 1) | ((now >> 2) & 2));
		*dy = pointer_quadrature((old & 1) | ((old >> 1) & 2),
			(now & 1) | ((now >> 1) & 2));
		break;
	case POINTER_TRAKBALL:
		if ((old ^ now) & 2)
			*dx = now & 1 ? 1 : -1;
		if ((old ^ now) & 8)
			*dy = now & 4 ? 1 : -1;
		break;
	}
}

#endif
