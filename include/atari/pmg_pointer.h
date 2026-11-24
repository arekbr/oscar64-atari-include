#ifndef ATARI_PMG_POINTER_H
#define ATARI_PMG_POINTER_H

#include "pmg_cursor.h"
#include "pointer.h"

#define PMG_POINTER_ST       POINTER_ST
#define PMG_POINTER_AMIGA    POINTER_AMIGA
#define PMG_POINTER_TRAKBALL POINTER_TRAKBALL
#define PMG_POINTER_TABLET   3
#define PMG_POINTER_LEFT     POINTER_LEFT
#define PMG_POINTER_RIGHT    2

struct APmgPointer
{
	APmgCursor * cursor;
	int dx, dy;
	byte type, port, previous, buttons;
};

// Connect to an initialized PMG cursor. Port is zero or one.
bool pmg_pointer_init(APmgPointer * pointer, APmgCursor * cursor, byte type, byte port);

// Consume a raw port nibble. Call this often for the relative devices.
// Two-bit quadrature jumps are discarded. This call never writes PMG RAM.
void pmg_pointer_sample(APmgPointer * pointer, byte pins);
void pmg_pointer_poll(APmgPointer * pointer);

// Scale a tablet sample to the cursor box. A lifted pen leaves it in place.
bool pmg_pointer_tablet(APmgPointer * pointer, byte pot_x, byte pot_y);

// Apply the accumulated motion or read the tablet and update button state.
// Call from the foreground. Relative-device samples use the same context.
bool pmg_pointer_update(APmgPointer * pointer);

#pragma compile("pmg_pointer.c")

#endif
