#ifndef ATARI_POKEY_H
#define ATARI_POKEY_H

#include "types.h"

// POKEY ($D200-$D20F). Use pokey for writes and pokeyr for reads.

// AUDC: bits 0..3 volume, bit 4 volume only, bits 5..7 distortion
#define AUDC_VOLUME_ONLY	0x10
#define AUDC_PURE_TONE		0xa0

// AUDCTL
#define AUDCTL_CLOCK_15KHZ	0x01	// 15 kHz base clock instead of 64 kHz
#define AUDCTL_HIPASS_2_4	0x02	// channel 2 high-pass filter clocked by channel 4
#define AUDCTL_HIPASS_1_3	0x04	// channel 1 high-pass filter clocked by channel 3
#define AUDCTL_JOIN_3_4		0x08	// channels 3+4 as 16-bit counter
#define AUDCTL_JOIN_1_2		0x10	// channels 1+2 as 16-bit counter
#define AUDCTL_CH3_179MHZ	0x20	// channel 3 clocked at 1.79 MHz
#define AUDCTL_CH1_179MHZ	0x40	// channel 1 clocked at 1.79 MHz
#define AUDCTL_POLY9		0x80	// 9-bit pseudo-random counter instead of 17-bit

// IRQEN (write) / IRQST (read, bit = 0 when interrupt asserted)
// IRQEN is write-only: OS keeps shadow copy in os_pokmsk ($10).
#define IRQ_TIMER1			0x01
#define IRQ_TIMER2			0x02
#define IRQ_TIMER4			0x04
#define IRQ_SEROUT_DONE		0x08
#define IRQ_SEROUT_NEED		0x10
#define IRQ_SERIN			0x20
#define IRQ_KEY				0x40
#define IRQ_BREAK			0x80

struct POKEY
{
	struct POKEYAudio
	{
		volatile byte	audf;		// frequency (divider)
		volatile byte	audc;		// Distortion and volume
	}	aud[4];						// $D200 channels 1..4 => aud[0..3]
	volatile byte	audctl;		// $D208
	volatile byte	stimer;		// $D209 any write restarts counters
	volatile byte	skrest;		// $D20A clears error bits in SKSTAT
	volatile byte	potgo;		// $D20B starts potentiometer measurement
	byte			unused;
	volatile byte	serout;		// $D20D byte sent via SIO
	volatile byte	irqen;		// $D20E Interrupt enable mask
	volatile byte	skctl;		// $D20F keyboard and serial port control
};

struct POKEYRead
{
	volatile byte	pot[8];		// $D200 potentiometer values
	volatile byte	allpot;		// $D208 potentiometer measurement status
	volatile byte	kbcode;		// $D209 last key code
	volatile byte	random;		// $D20A pseudo-random number
	byte			unused[2];
	volatile byte	serin;		// $D20D byte received via SIO
	volatile byte	irqst;		// $D20E interrupt status
	volatile byte	skstat;		// $D20F keyboard and serial port status
};

// Write registers
#define pokey	(*((struct POKEY *)0xd200))
// Read registers
#define pokeyr	(*((struct POKEYRead *)0xd200))

#endif
