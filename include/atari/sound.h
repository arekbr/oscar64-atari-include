#ifndef ATARI_SOUND_H
#define ATARI_SOUND_H

#include "types.h"
#include "pokey.h"

// POKEY audio with Atari BASIC SOUND parameters.
//
//	sound_init();
//	sound(0, 121, SOUND_PURE, 8);   // First channel, about 260 Hz on PAL.
//	...
//	sound_off();
//
// With AUDCTL = 0, the pure-tone frequency is clock / (2 * (freq + 1)).
// The clock is about 63337 Hz on PAL and 63921 Hz on NTSC.

// Distortion values follow Atari BASIC. AUDC stores dist / 2 in bits 5..7
// and volume in bits 0..3.
#define SOUND_POLY5_17		0	// $00, 5-bit and 17-bit polynomial counters
#define SOUND_POLY5			2	// $20, also selected by distortion 6 ($60)
#define SOUND_POLY5_4		4	// $40, 5-bit and 4-bit polynomial counters
#define SOUND_POLY17		8	// $80, 17-bit noise (9-bit with AUDCTL_POLY9)
#define SOUND_PURE			10	// $A0, pure tone, also selected by distortion 14 ($E0)
#define SOUND_POLY4			12	// $C0, 4-bit noise

// Selects the 64 kHz clock and independent channels (AUDCTL = 0),
// enables POKEY operation (SKCTL = 3), and mutes all four channels.
void sound_init(void);

// Starts or updates channel 0..3. freq is the AUDF divider (0..255),
// dist is an even value from 0 to 14, and vol ranges from 0 to 15.
void sound(byte ch, byte freq, byte dist, byte vol);

// Mutes all four channels.
void sound_off(void);

#pragma compile("sound.c")

#endif
