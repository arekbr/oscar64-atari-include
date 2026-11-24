#ifndef ATARI_PTIMER_H
#define ATARI_PTIMER_H

#include "types.h"
#include "irq.h"

// POKEY IRQ timers 1, 2 and 4 use their corresponding audio channels.
// Join channels 1 and 2 for a 16-bit timer 2, or 3 and 4 for timer 4.
// Configure the clock and any channel joining in AUDCTL before starting.
//
// The OS must handle IRQs. Declare each callback __interrupt and
// leave IRQs masked until it returns. The wrapper saves the compiler
// temporaries used by the callback. An NMI handler that uses those
// temporaries must preserve them before returning to a timer callback.
//
// Call the timer API from the foreground or a timer callback. Defer
// changes requested by an NMI to the foreground. An NMI could otherwise
// replace a handler address while a timer entry is reading its two bytes.
//
// With the 64 kHz or 15 kHz clock, the divider is AUDF + 1. At the
// CPU clock rate, add 4 to a single-channel divider or 7 to a joined
// divider. Audio and serial I/O can change the clock or dividers.
// Reapply the timer settings after another user changes those registers.

#define PTIMER_1	1
#define PTIMER_2	2
#define PTIMER_4	4

// Install f and save the previous vector on the first attach.
// Attaching again replaces f. The IRQ enable bit stays as it was.
void ptimer_attach(byte t, IRQHandler f);

// Attach only if the library has no callback and the OS IRQ bit is clear.
bool ptimer_claim(byte t, IRQHandler f);

// Disable the timer IRQ and restore its saved vector if attached.
void ptimer_detach(byte t);

// Set or clear this timer's interrupt bit in POKMSK and IRQEN.
void ptimer_enable(byte t);
void ptimer_disable(byte t);

// Set AUDF for the selected timer.
void ptimer_period(byte t, byte audf);

// Set the low and high AUDF bytes for a joined pair. Select PTIMER_2
// or PTIMER_4. Setting timer 2 disables the timer 1 interrupt.
void ptimer_period16(byte t, word div);

// Write STIMER to restart all four audio counters.
void ptimer_restart_all(void);

#pragma compile("ptimer.c")

#endif
