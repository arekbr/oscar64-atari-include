#ifndef ATARI_GRAPHICS_H
#define ATARI_GRAPHICS_H

#include "cio.h"

#define GR_SPLIT	0x10	// Keep the four-line E: text window.
#define GR_NOCLEAR	0x20	// Ask the OS to skip clearing on open.

struct AGraphicsMode
{
	word	width;		// Pixels or characters per row.
	byte	height;		// Rows on a full screen.
	byte	split_height;	// Rows above the text window, or zero.
	byte	bits;		// Bits per pixel, or zero for a text mode.
};

struct AGraphics
{
	word	width;
	byte	height;
	byte	mode;
	byte	channel;
	byte	handler;
	byte	bits;
};

// Return a layout for XL/XE GRAPHICS modes 0 through 15, or null.
const AGraphicsMode * gr_mode_info(byte mode);

// Open S: on a free CIO channel from 1 through 7, with optional GR_* flags.
// Leave *screen unchanged on error. The OS allocates the screen in high RAM.
// Keep the program, heap and stack below the new screen and display list.
// Keep E: open on channel 0 when using a split screen.
byte gr_open(AGraphics * screen, byte channel, byte mode, byte flags);

// Close the channel and release the view. The OS keeps the displayed mode.
byte gr_close(AGraphics * screen);

// Put a pixel or an ATASCII character at x, y. Pixel values use the low
// bits for the mode. Text modes use the display handler's control codes.
byte gr_plot(const AGraphics * screen, word x, byte y, byte value);

// Read one pixel or character through S:. Keep *value unchanged on error.
byte gr_locate(const AGraphics * screen, word x, byte y, byte * value);

// Draw from the previous plot or draw position. Use only in bitmap modes.
byte gr_draw_to(const AGraphics * screen, word x, byte y, byte color);

// Clear the screen and any text window using the OS clear code.
byte gr_clear(const AGraphics * screen);

// These functions use the OS display handler and return CIO status codes.
// Plot, read and draw coordinates outside the view return CIO_CURSOR.
// Open one screen at a time. Reopening S: elsewhere invalidates the view.

#pragma compile("graphics.c")

#endif
