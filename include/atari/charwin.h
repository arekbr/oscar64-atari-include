#ifndef ATARI_CHARWIN_H
#define ATARI_CHARWIN_H

#include "types.h"

// Text windows are views of caller-owned character screen memory. Keep the
// memory valid while using a window. The display list determines how each
// screen byte is displayed.
//
// Pitch is the number of bytes between row starts. It may be 1..255, including
// any padding in the underlying screen. Use the actual screen layout. A
// subwindow inherits its parent's pitch
// and clips to the parent's bounds. A window is empty if its width or height is
// zero, or if its origin is outside its parent. Empty windows read no cells,
// write nothing, and cwin_edit returns ATASCII_ESC immediately.
//
// Positions passed to window operations are zero-based and relative to that
// window. The init functions do not clear cells. They reset the cursor to
// (0, 0) and select hidden XOR style. Hide a visible cursor explicitly
// before reinitializing or discarding its window so the cell is restored.
//
// Text functions translate ATASCII to or from screen codes. The _raw variants
// use screen codes directly. Conversion preserves bit 7. Ordinary writes
// treat control bytes literally.
// Use cwin_edit* to handle editor keys and cwin_console_* for line breaks and
// scrolling. Strings end at byte 0, including raw strings, where 0 is the
// screen code for space. Use counted or rectangle calls for data containing 0.
//
// A visible cursor modifies one cell. XOR style toggles bit 7 (inverse video
// in ANTIC mode 2). In modes 4..7 it affects color, so use a replacement screen
// code when the XOR cursor is hard to see. Replacement style saves and restores
// the cell's original byte. Repeated show or hide calls have no further effect.
// Changing styles while visible restores the old cell and applies the new style.
// Reads through the same window return the byte under the cursor. Before
// accessing that cell through another overlapping window or directly, hide the
// cursor in the window that owns it.
//
// Pass buffers separate from screen memory and the CharWin structure. Use this
// API only from the main program. Operations share scratch storage and cannot
// be interleaved from interrupt handlers. Hide the OS cursor before editing
// E:'s screen memory, and keep E: output away from the windows. The OS editor
// can otherwise restore its saved character over a window's output.

#define CWIN_COLS		40
#define CWIN_ROWS		24

#define CWIN_CURSOR		0x01		// fl: cursor visible
#define CWIN_FULL		0x02		// fl: last cell written; console wrap pending
#define CWIN_CODE		0x04		// fl: replacement cursor style (0 selects XOR)

struct CharWin
{
	byte			sx, sy, wx, wy;		// screen position and dimensions (0 for an empty window)
	byte			cx, cy, fl;
	byte			pitch;			// bytes between screen rows
	volatile byte *	sp;				// top-left cell in screen memory
	byte			cb;				// replacement code while hidden, saved cell while visible
};

// Convert a single character.
byte cwin_a2s(byte ch);			// ATASCII to screen code
byte cwin_s2a(byte ch);			// screen code to ATASCII

// cwin_init_sub accepts win == parent to replace the view with a subwindow.
void cwin_init_screen(CharWin * win, volatile byte * screen, byte pitch, byte rows);	// whole screen, pitch by rows
void cwin_init_sub(CharWin * win, CharWin * parent, byte sx, byte sy, byte wx, byte wy);	// offset and size within parent
void cwin_init(CharWin * win, volatile byte * screen, byte sx, byte sy, byte wx, byte wy);	// window on a 40-column, 24-row screen
void cwin_clear(CharWin * win);								// fill with spaces, move to (0, 0), clear CWIN_FULL
void cwin_fill(CharWin * win, byte ch);						// fill with an ATASCII byte, leaving the cursor in place

void cwin_cursor_show(CharWin * win, bool show);
void cwin_cursor_code(CharWin * win, byte cc);				// use raw screen code cc as the cursor
void cwin_cursor_xor(CharWin * win);						// use XOR cursor style
void cwin_cursor_move(CharWin * win, byte cx, byte cy);		// clamp to the window and clear CWIN_FULL
bool cwin_cursor_left(CharWin * win);						// false if blocked at the left edge
bool cwin_cursor_right(CharWin * win);
bool cwin_cursor_up(CharWin * win);
bool cwin_cursor_down(CharWin * win);
bool cwin_cursor_forward(CharWin * win);					// wrap to the next row, false at the last cell
bool cwin_cursor_backward(CharWin * win);					// wrap to the previous row, false at the first cell
bool cwin_cursor_newline(CharWin * win);					// move to the next row, false on the last row

// Write from the cursor, wrapping at the row edge. Writing the last cell
// leaves the cursor there and sets CWIN_FULL. Further writes stop until the
// cursor moves or the window is cleared. Counted writes return bytes written.
void cwin_put_char(CharWin * win, byte ch);
word cwin_put_chars(CharWin * win, const byte * chars, word num);	// returns the number of bytes written
word cwin_put_string(CharWin * win, const char * str);
void cwin_put_char_raw(CharWin * win, byte ch);
word cwin_put_chars_raw(CharWin * win, const byte * chars, word num);
word cwin_put_string_raw(CharWin * win, const char * str);

// Read or write within one row without moving the cursor. Transfers are
// clipped at the row edge and return a count of 0 for an out-of-range origin.
void cwin_putat_char(CharWin * win, byte x, byte y, byte ch);
word cwin_putat_chars(CharWin * win, byte x, byte y, const byte * chars, word num);
word cwin_putat_string(CharWin * win, byte x, byte y, const char * str);
void cwin_putat_char_raw(CharWin * win, byte x, byte y, byte ch);
word cwin_putat_chars_raw(CharWin * win, byte x, byte y, const byte * chars, word num);
word cwin_putat_string_raw(CharWin * win, byte x, byte y, const char * str);

byte cwin_getat_char(CharWin * win, byte x, byte y);		// 0 outside the window
word cwin_getat_chars(CharWin * win, byte x, byte y, byte * chars, word num);
byte cwin_getat_char_raw(CharWin * win, byte x, byte y);
word cwin_getat_chars_raw(CharWin * win, byte x, byte y, byte * chars, word num);
void cwin_read_string(CharWin * win, char * buffer);		// whole window, wx * wy + 1 bytes including NUL

// Rectangles clip to the window and leave the cursor in place. The buffer
// row stride is the requested width w. Cells outside the clipped area stay
// unchanged in both the screen and the buffer.
void cwin_put_rect(CharWin * win, byte x, byte y, byte w, byte h, const byte * chars);
void cwin_put_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, const byte * chars);
void cwin_get_rect(CharWin * win, byte x, byte y, byte w, byte h, byte * chars);
void cwin_get_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, byte * chars);
void cwin_fill_rect(CharWin * win, byte x, byte y, byte w, byte h, byte ch);
void cwin_fill_rect_raw(CharWin * win, byte x, byte y, byte w, byte h, byte ch);

void cwin_insert_char(CharWin * win, byte ch);		// shift row right, drop its last cell, keep cursor in place
void cwin_insert_char_raw(CharWin * win, byte ch);
void cwin_delete_char(CharWin * win);				// shift row left and put a space in its last cell

// Scroll without filling the exposed strip. Zero or a distance at least as
// large as the window dimension leaves the contents unchanged.
void cwin_scroll_left(CharWin * win, byte by);
void cwin_scroll_right(CharWin * win, byte by);
void cwin_scroll_up(CharWin * win, byte by);
void cwin_scroll_down(CharWin * win, byte by);

// Handle one ATASCII key. Returns 1 for a recognized key, even if blocked
// at an edge, and 0 for RETURN, ESC, or an unsupported control code. Arrow
// keys stop at row or column edges. Backspace moves back and erases. INSERT
// inserts a space, DELETE removes a cell, and printable bytes use put_char.
byte cwin_edit_char(CharWin * win, byte ch);
// Show the cursor and read keys until RETURN or ESC. Hide the cursor and
// return the terminating ATASCII code (ATASCII_EOL or ATASCII_ESC).
byte cwin_edit(CharWin * win);

// Console output wraps and scrolls up when it needs another row, filling
// the new row with spaces. A newline ('\n' or ATASCII_EOL) starts one new row
// even if the window is full.
void cwin_console_write_char(CharWin * win, byte ch);
void cwin_console_write_string(CharWin * win, const char * str);
void cwin_console_newline(CharWin * win);
void cwin_console_clear(CharWin * win);

#pragma compile("charwin.c")

#endif
