#ifndef ATARI_DLIC_H
#define ATARI_DLIC_H

#include "types.h"
#include "irq.h"

// Run one slot per DLI, in display-list order. The immediate VBI selects
// the first slot each frame, even when CRITIC causes the OS to skip deferred
// VBI. Extra DLIs use an empty RTI handler. The chain owns VDSLST and hooks
// VVBLKI while running. A deferred vbi_set handler can coexist. Install
// a raw dli_set handler after stopping the chain.
//
// Slot code lives in a single RAM page so advancing VDSLST takes one byte
// write. Keep DLI lines clear of vertical blank: a slot interrupted before
// that write can overwrite the VBI's selection of the first slot.
//
// dlic_call requires an __interrupt callback. The slot must finish before
// the next DLI. The slot saves A/X/Y, the callback preserves the zero-page
// workspace it uses, and the wrapper saves tmpy and WORK+4..8 and clears
// decimal mode.
// The call overhead can delay a register write by one or more scan lines.

#define DLIC_CODE	42		// Code bytes reserved for each slot.
#define DLIC_SLOTS	6		// Six slots and the empty handler fit in one page.
#define DLIC_OPS	6
#define DLIC_FULL	255		// Operation could not be added.

typedef struct DLISlot
{
	byte	at;					// Code offset, or zero before allocation.
	byte	len;				// Code length.
	byte	nops;				// Operation count.
	byte	calls;				// Bit n marks a callback operation.
	bool	bad;				// Slot overflowed while building.
	byte	ofs[DLIC_OPS];		// Operation offsets within the slot.
} DLISlot;

// Begin building a zero-initialized slot with static storage. Allocate on
// first use and return false if the pool is full. With wsync enabled,
// operations start at the next scan line. Build or rebuild while stopped.
bool dlic_begin(DLISlot * s, bool wsync);

// Append a byte write. Return its operation index or DLIC_FULL.
byte dlic_write(DLISlot * s, volatile void * addr, byte data);

// Append an __interrupt callback. Return its operation index or DLIC_FULL.
byte dlic_call(DLISlot * s, IRQHandler f);

// Finish the slot. Return false if it overflowed or has no space.
bool dlic_end(DLISlot * s);

// Patch a write's data byte, including while running. Return false for
// a callback or an invalid operation index.
bool dlic_data(DLISlot * s, byte op, byte data);

// Patch a write's address while stopped. Return false while running or
// if the index does not name a write.
bool dlic_addr(DLISlot * s, byte op, volatile void * addr);

// Link completed slots while stopped, in DLI order. The last slot leads
// to the empty handler. A count of zero leaves the existing links in place.
void dlic_chain(DLISlot ** slots, byte n);

// Select the first completed slot and enable DLI at the frame boundary.
// While running, replace the first slot and retain the saved VVBLKI.
void dlic_start(DLISlot * first);

// Disable DLI, keep VBI enabled, and restore the saved VVBLKI.
// The caller can reinstall a previous DLI handler when needed.
void dlic_stop(void);

#pragma compile("dlic.c")

#endif
