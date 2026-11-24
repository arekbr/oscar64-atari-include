#include "pmg_pointer.h"
#include "pointer_decode.h"
#include "input.h"
#include "pia.h"

static byte port_pins(byte port)
{
	byte raw = pia.porta;
	return port ? raw >> 4 : raw & 15;
}

bool pmg_pointer_init(APmgPointer * pointer, APmgCursor * cursor, byte type, byte port)
{
	if (!pointer || !cursor || type > PMG_POINTER_TABLET || port > 1)
		return false;
	pointer->cursor = cursor;
	pointer->type = type;
	pointer->port = port;
	pointer->previous = port_pins(port);
	pointer->dx = pointer->dy = 0;
	pointer->buttons = 0;
	return true;
}

static int accumulate(int total, int step)
{
	int next = total + step;
	return next < -255 ? -255 : next > 255 ? 255 : next;
}

void pmg_pointer_sample(APmgPointer * pointer, byte pins)
{
	byte old = pointer->previous;
	byte now = pins & 15;
	if (old == now)
		return;
	pointer->previous = now;
	sbyte dx, dy;
	pointer_decode(pointer->type, old, now, &dx, &dy);
	pointer->dx = accumulate(pointer->dx, dx);
	pointer->dy = accumulate(pointer->dy, dy);
}

void pmg_pointer_poll(APmgPointer * pointer)
{
	pmg_pointer_sample(pointer, port_pins(pointer->port));
}

// Keep the unsigned coordinate scaling independent of constant samples.
#pragma optimize(push, 2, noinline, noconstparams)
bool pmg_pointer_tablet(APmgPointer * pointer, byte pot_x, byte pot_y)
{
	if (!pot_x || !pot_y || pot_x >= 228 || pot_y >= 228)
		return true;
	const APmgCursorConfig * box = &pointer->cursor->config;
	int x = box->left + (word)(pot_x - 1) * (box->right - box->left) / 226u;
	int y = box->top + (word)(227 - pot_y) * (box->bottom - box->top) / 226u;
	return pmg_cursor_move(pointer->cursor, x, y);
}
#pragma optimize(pop)

bool pmg_pointer_update(APmgPointer * pointer)
{
	if (pointer->type == PMG_POINTER_TABLET)
	{
		byte mask = ~port_pins(pointer->port);
		pointer->buttons = (mask & 5 ? PMG_POINTER_LEFT : 0) | (mask & 8 ? PMG_POINTER_RIGHT : 0);
		byte index = pointer->port << 1;
		return pmg_pointer_tablet(pointer, pad_read(index), pad_read(index + 1));
	}
	pointer->buttons = joy_fire(pointer->port) ? PMG_POINTER_LEFT : 0;
	APmgCursor * cursor = pointer->cursor;
	if (!pmg_cursor_move(cursor, cursor->x + pointer->dx, cursor->y + pointer->dy))
		return false;
	pointer->dx = pointer->dy = 0;
	return true;
}
