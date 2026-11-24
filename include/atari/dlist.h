#ifndef ATARI_DLIST_H
#define ATARI_DLIST_H

#include "types.h"
#include "antic.h"

// Build in caller-owned RAM. Each append returns the next free byte.
// Keep a display list segment within 1 KB and each ANTIC fetch within 4 KB.
// Use JMP between list pages and LMS at a screen-memory boundary.
// Write ANTIC and GTIA DMA data through volatile pointers.

#define DL_BLANK(n)		((byte)(((n) - 1) << 4))	// Encode a blank instruction for 1 to 8 scanlines.

// Append n blank scanlines in groups of up to eight.
volatile byte * dl_blank(volatile byte * dl, byte lines);

// Append count mode lines. mode may include DLI or scroll flags.
volatile byte * dl_mode(volatile byte * dl, byte mode, byte count);

// Append one mode line with a new screen memory address.
volatile byte * dl_lms(volatile byte * dl, byte mode, const volatile void * screen);

// Finish the list with a jump that waits for vertical blank.
volatile byte * dl_jvb(volatile byte * dl, const volatile void * start);

// Continue at start without waiting for vertical blank.
volatile byte * dl_jmp(volatile byte * dl, const volatile void * start);

// Wait for a frame tick, then set the OS display list shadow.
void dl_set(const volatile void * dl);

// True only for a mode instruction with LMS, excluding blank lines and JVB.
bool dl_is_lms(const volatile byte * dl);

// Return the instruction length in bytes. LMS, JMP and JVB take three bytes.
byte dl_size(const volatile byte * dl);

// Follow JMP and return null at JVB. Pass a valid instruction and bound
// the walk if JMP can loop.
volatile byte * dl_next(volatile byte * dl);

#pragma compile("dlist.c")

#endif
