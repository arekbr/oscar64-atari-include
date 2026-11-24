#ifndef ATARI_INPUT_H
#define ATARI_INPUT_H

#include "types.h"
#include "os.h"
#include "gtia.h"

// Joystick ports are 0 and 1. Set bits mark held directions.
#define JOY_UP		0x01
#define JOY_DOWN	0x02
#define JOY_LEFT	0x04
#define JOY_RIGHT	0x08

// Raw keyboard codes, before ATASCII translation.
#define KEY_NONE	0xff
#define KEY_SHIFT	0x40	// Shift bit in code.
#define KEY_CTRL	0x80	// Control bit in code.

// Return the held directions as a JOY_* mask.
byte joy_read(byte n);

// Return true while the fire button is held.
bool joy_fire(byte n);

// Paddles 0 and 1 use joystick port 0. Paddles 2 and 3 use port 1.
// The OS updates paddle shadows during vertical blank.
// Read after the first VBI. A value of 228 can indicate no paddle.
byte pad_read(byte n);

// Return true while the paddle trigger is held.
bool pad_fire(byte n);

// Return pressed keys as a CONSOL_* mask. Leave bits 0..2 clear on
// speaker writes so the key lines remain readable.
byte cons_keys(void);

// Read and clear HELP atomically, preserving the CPU interrupt mask.
// Zero means no event. Some OS revisions include SHIFT or CONTROL.
byte key_help(void);

// Return true if the OS keyboard buffer contains a key.
bool key_pressed(void);

// Wait for a raw keyboard code and clear the OS buffer.
byte key_get(void);

// ATASCII editor control characters
#define ATASCII_EOL		0x9b	// RETURN.
#define ATASCII_ESC		0x1b
#define ATASCII_BS		0x7e	// Back Space.
#define ATASCII_TAB		0x7f
#define ATASCII_UP		0x1c
#define ATASCII_DOWN	0x1d
#define ATASCII_LEFT	0x1e
#define ATASCII_RIGHT	0x1f
#define ATASCII_DELETE	0xfe	// Delete the character under the cursor.
#define ATASCII_INSERT	0xff	// Insert a space at the cursor.

// Wait for a character through the OS keyboard handler. The OS processes
// Caps Lock, inverse video and the key click.
// RETURN is reported as ATASCII_EOL.
byte key_getc(void);

// Translate a raw keyboard code using the OS key table and Caps Lock state.
// This does not wait for input, change OS state or apply inverse video.
// Return zero when the key has no character.

byte key_atascii(byte code);

#pragma compile("input.c")

#endif
