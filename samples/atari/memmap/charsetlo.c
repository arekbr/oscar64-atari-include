// Run this sample as a bare XEX. Many DOS handlers use this low RAM.
#define FONT_START 0x0800
#define FONT_END   0x0c00
#define FONT_HELLO
#include "charset_address.h"

int main(void)
{
	// These are display codes for the custom font in this XEX segment.
	static const byte message[] = {32, 8, 5, 12, 12, 15, 32, 23, 15, 18, 12, 4};
	con_clrscr();
	con_cursor(0);
	byte previous = os_chbas;
	os_chbas = (word)tiles >> 8;
	volatile byte * cells = (volatile byte *)os_savmsc;
	for (byte y = 0; y < 10; ++y)
	{
		cells[40 * y] = '0' + y;
		for (byte i = 0; i < sizeof(message); ++i)
			cells[40 * y + i + 1] = message[i];
	}
	while (!(cons_keys() & CONSOL_START) && !key_pressed())
		os_wait_frame();
	os_chbas = previous;
	con_cursor(1);
	return 0;
}
