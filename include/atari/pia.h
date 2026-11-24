#ifndef ATARI_PIA_H
#define ATARI_PIA_H

#include "types.h"

// PIA 6520, $D300..$D303. On XL/XE: porta = joysticks 0 and 1, portb = memory control.

// PORTB on XL/XE (when PBCTL set to data register)
#define PORTB_OS_ROM		0x01	// 1 = system ROM enabled
#define PORTB_BASIC_OFF		0x02	// 1 = BASIC disabled
#define PORTB_BANK_MASK		0x0c	// 130XE: 16 kB bank select in $4000..$7FFF
#define PORTB_CPU_EXT_OFF	0x10	// 130XE: 0 = CPU sees extended bank
#define PORTB_ANTIC_EXT_OFF	0x20	// 130XE: 0 = ANTIC sees extended bank
#define PORTB_SELFTEST_OFF	0x80	// 1 = self-test ROM disabled

struct PIA
{
	volatile byte	porta;		// $D300 joysticks 0..1 (bit = 0 when direction pressed)
	volatile byte	portb;		// $D301 XL/XE: memory configuration
	volatile byte	pactl;		// $D302 port A control
	volatile byte	pbctl;		// $D303 port B control
};

#define pia	(*((struct PIA *)0xd300))

#endif
