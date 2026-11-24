#ifndef ATARI_POINTER_H
#define ATARI_POINTER_H

#include "types.h"

#define POINTER_ST       0
#define POINTER_AMIGA    1
#define POINTER_TRAKBALL 2
#define POINTER_LEFT     1

struct APointerMotion
{
	sbyte dx, dy;
	byte buttons;
};

// Sample a relative device on port 0 or 1 with POKEY timer 2. Reserve
// standalone channel 2 at the 64 kHz clock (AUDCTL bits 0 and 4 clear).
// Pass the previous write-only AUDF2/AUDC2 settings for restoration.
// Keep the channel and timer reserved while active. The OS handles IRQs.
// Start fails if a timer 2 IRQ or library handler is already in use.
bool pointer_irq_start(byte type, byte port, byte saved_audf, byte saved_audc);

// Read and clear the accumulated motion (each axis saturates at +/-127)
// and sample the trigger. Call regularly from the foreground. After the
// OS critical section ends, this also restarts a suspended sampler.
bool pointer_irq_take(APointerMotion * motion);

// Disable sampling, release the timer and restore its vector and sound.
void pointer_irq_stop(void);

#pragma compile("pointer.c")

#endif
