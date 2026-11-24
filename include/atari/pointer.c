#include "pointer.h"
#include "pointer_decode.h"
#include "input.h"
#include "pia.h"
#include "pokey.h"
#include "ptimer.h"

#define POINTER_AUDF 63
#define OS_CRITIC (*(volatile byte *)0x0042)

static byte old_audf, old_audc;
static volatile byte pointer_type, pointer_port, previous;
static volatile bool active, paused;
static volatile sbyte pending_x, pending_y;

static __forceinline byte read_pins(byte port)
{
	byte pins = pia.porta;
	return port ? pins >> 4 : pins & 15;
}

static __forceinline sbyte add_step(sbyte sum, sbyte step)
{
	if ((sum == 127 && step > 0) || (sum == -127 && step < 0))
		return sum;
	return sum + step;
}

static __interrupt void sample_irq(void)
{
	if (OS_CRITIC)
	{
		paused = true;
		os_pokmsk &= (byte)~IRQ_TIMER2;
		pokey.irqen = os_pokmsk;
		return;
	}
	if (paused)
		return;
	byte now = read_pins(pointer_port);
	byte old = previous;
	if (now == old)
		return;
	previous = now;
	sbyte dx, dy;
	pointer_decode(pointer_type, old, now, &dx, &dy);
	pending_x = add_step(pending_x, dx);
	pending_y = add_step(pending_y, dy);
}

bool pointer_irq_start(byte type, byte port, byte saved_audf, byte saved_audc)
{
	if (type > POINTER_TRAKBALL || port > 1 || active || OS_CRITIC ||
		!ptimer_claim(PTIMER_2, sample_irq))
		return false;

	old_audf = saved_audf;
	old_audc = saved_audc;
	pointer_type = type;
	pointer_port = port;
	previous = read_pins(port);
	pending_x = pending_y = 0;
	paused = false;
	active = true;
	pokey.aud[1].audc = 0;
	ptimer_period(PTIMER_2, POINTER_AUDF);
	ptimer_enable(PTIMER_2);
	return true;
}

bool pointer_irq_take(APointerMotion * motion)
{
	if (!active || !motion)
		return false;

	__asm volatile {
		php
		sei
	}
	sbyte dx = pending_x;
	sbyte dy = pending_y;
	pending_x = pending_y = 0;
	if (!OS_CRITIC && (paused || !(os_pokmsk & IRQ_TIMER2)))
	{
		previous = read_pins(pointer_port);
		paused = false;
		pokey.aud[1].audc = 0;
		ptimer_period(PTIMER_2, POINTER_AUDF);
		ptimer_enable(PTIMER_2);
	}
	__asm volatile {
		plp
	}

	motion->dx = dx;
	motion->dy = dy;
	motion->buttons = joy_fire(pointer_port) ? POINTER_LEFT : 0;
	return true;
}

void pointer_irq_stop(void)
{
	if (active)
	{
		ptimer_detach(PTIMER_2);
		pokey.aud[1].audf = old_audf;
		pokey.aud[1].audc = old_audc;
		active = false;
	}
}
