#ifndef ATARI_IRQ_H
#define ATARI_IRQ_H

#include "types.h"

// Display list interrupts and deferred vertical blank callbacks.
//
// Declare a display list interrupt handler as __hwinterrupt. The OS
// dispatches through VDSLST without saving A, X or Y. The handler saves
// the required registers and returns with RTI.
//
// __hwinterrupt void dli(void) { antic.wsync = 0; gtia.colbk = 0x34; }
// dli_set(dli);
//
// Declare a deferred vertical blank handler as __interrupt. The OS has
// already saved the CPU registers. The wrapper calls the handler and
// then exits through XITVBV. The OS calls it after copying register shadows.
// Critical sections can cause the OS to skip a deferred callback.
//
// __interrupt void vbi(void) { frames++; }
// vbi_set(vbi);

typedef void (* IRQHandler)(void);

// Install a display list interrupt handler. DLI is masked while the vector changes.
void dli_set(IRQHandler f);

// Disable display list interrupts and keep VBI enabled.
void dli_off(void);

// Install a deferred vertical blank handler through SETVBV.
void vbi_set(IRQHandler f);

// Restore the default deferred vertical blank exit.
void vbi_off(void);

#pragma compile("irq.c")

#endif
