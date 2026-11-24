#ifndef ATARI_SAMPLE_CHARSET_H
#define ATARI_SAMPLE_CHARSET_H

#include <atari/console.h>
#include <atari/input.h>

#define FONT_SIZE 1024

static void show_font(const volatile byte * font)
{
	con_clrscr();
	con_cursor(0);
	volatile byte * cells = (volatile byte *)os_savmsc;
	for (word i = 0; i < 40 * 24; ++i)
		cells[i] = i;
	byte previous = os_chbas;
	os_chbas = (word)font >> 8;
	while (!(cons_keys() & CONSOL_START) && !key_pressed())
		os_wait_frame();
	os_chbas = previous;
	con_cursor(1);
}
#endif
