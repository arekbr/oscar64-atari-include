#ifndef ATARI_LOWRAM_H
#define ATARI_LOWRAM_H
#include "types.h"
// Low RAM for a standalone program on a stock 64 KB XL/XE. Call
// lr_takeover from foreground code after the DOS loader reaches RUN.
// XEX load records, INIT routines and CRT startup must leave the borrowed
// ranges untouched. Initialize the program's buffers after takeover succeeds.
#define LR_BASE ((volatile byte *)0x0800)
#define LR_SIZE 6144u
// Optional former GRAPHICS 0 display allocation. The same ownership
// and initialization rules apply. Keep BASIC disabled.
#define LR_SCREEN_ADDR 0xbc00u
#define LR_SCREEN_SIZE 1024u
#define LR_SCREEN_BASE ((volatile byte *)LR_SCREEN_ADDR)
// Call before installing application interrupt handlers. Start with the OS
// visible, BASIC and self-test disabled, normal PIA data directions and
// data access on both ports, port IRQs disabled, and no pending port B
// interrupt flags. RAMTOP must be $A0 or $C0 and VIMIRQ and VKEYBD must
// point into the OS ROM. The caller supplies a stock 64 KB machine.
//
// Stop display DMA, restore the OS VBI vectors, clear the OS timer counters,
// and enable VBI and keyboard IRQ only. Preserve PORTB, RTC and the CPU I
// flag. The module leaves buffer initialization to the caller.
//
// Use neither DOS, CIO nor SIO after success. Leave the program with RESET,
// which starts a cold boot and ends ownership. Repeating a successful
// takeover returns true without changing the configuration.
bool lr_takeover(void);

// Report whether takeover has succeeded in this program run.
bool lr_active(void);
#pragma compile("lowram.c")
#endif
