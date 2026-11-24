#ifndef ATARI_CONSOLE_H
#define ATARI_CONSOLE_H

#include "types.h"

// Console I/O uses the E: screen editor, which the OS opens on IOCB 0.
// The editor echoes input and lets the user edit the line until RETURN.
// For immediate keyboard input, use key_getc() and key_pressed() in input.h.

// Read an edited line into buf, which must hold size bytes. Store up to
// size - 1 ATASCII characters, omit the ATASCII EOL and terminate with '\0'.
// Discard any excess input through the end of the line.
// Return the number of characters stored, or -1 if size < 2 or CIO fails.
int con_gets(char * buf, byte size);

// Return the next ATASCII character from an edited line, or -1 on CIO error.
// Wait for RETURN when starting a new line and report its ATASCII EOL as '\n'.
int con_getchar(void);

// Write formatted output to E:, translating '\n' to ATASCII EOL.
// Format and return value follow fmt_cprintf() in fmt.h.
int con_printf(const char * fmt, ...);

// E: screen editor controls. See input.h for other ATASCII control codes.
#define CH_CLR		0x7d	// Clear the screen.
#define CH_BELL		0xfd	// Ring the console bell.
#define CH_DELLINE	0x9c	// Delete the logical line at the cursor.
#define CH_INSLINE	0x9d	// Insert a blank row at the cursor.

// Screen helpers use GRAPHICS 0 (40 columns by 24 rows). The helpers that
// send editor control codes clear DSPFLG for the write and then restore it.

// Clear the screen and move the cursor to the left margin on row 0.
void con_clrscr(void);

// Move the cursor and refresh its display. Coordinates are zero-based
// and measure from the screen edges. Clamp x to 39 and y to 23.
void con_gotoxy(byte x, byte y);

// Return the current column (low byte of COLCRS) and row (ROWCRS).
byte con_wherex(void);
byte con_wherey(void);

// Clear from the cursor through column 39, leaving the cursor in place.
// Ignore coordinates outside the screen as reported by con_wherex() and
// con_wherey().
void con_clreol(void);

// Delete the logical line at the cursor, including any wrapped rows, and shift
// lower rows up. Move the cursor to the left margin on the first deleted row.
void con_delline(void);

// Insert a blank screen row at the cursor and move it to the left margin.
// Shift the previous contents down, dropping the bottom row. Inserting in
// the middle of a wrapped line splits it.
void con_insline(void);

// Ring the bell without moving the cursor.
void con_bell(void);

// Show the cursor if on is nonzero, or hide it if zero. Refresh it immediately.
void con_cursor(byte on);

// Set the text and background shadows. The OS copies them to GTIA at the next VBI.
// Colors encode (hue << 4) | luminance. See colors.h for named values.
// In GRAPHICS 0, text uses the background hue and the luminance of text.
void con_colors(byte text, byte bg);

// Set the border color shadow for the next VBI.
void con_border(byte color);

#pragma compile("console.c")

#endif
