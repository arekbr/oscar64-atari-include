#ifndef ATARI_SAMPLE_TEXT_GRID_H
#define ATARI_SAMPLE_TEXT_GRID_H

#include <atari/console.h>
#include <atari/input.h>

#define COLUMNS 40
#define ROWS 24
static volatile byte * cells;

static void text_begin(const char * title)
{
	con_clrscr();
	con_printf("%s\n", title);
	con_cursor(0);
	cells = (volatile byte *)os_savmsc;
}

static void tile(byte x, byte y, byte character)
{
	cells[(word)y * COLUMNS + x] = character - ' ';
}

static void text_end(void)
{
	con_gotoxy(2, 22);
	con_printf("START EXITS");
	while (!(cons_keys() & CONSOL_START))
		os_wait_frame();
	con_cursor(1);
}

#endif
