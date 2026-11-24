#ifndef ATARI_SAMPLE_CHARSET_ADDRESS_H
#define ATARI_SAMPLE_CHARSET_ADDRESS_H

#include "charset.h"

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})
#pragma section(font_data, 0)
#pragma region(font_data, FONT_START, FONT_END, , , {font_data})
#pragma data(font_data)
static const byte tiles[FONT_SIZE] = {
	#embed "tiles.fnt"
};
#pragma data(data)

#ifndef FONT_HELLO
int main(void)
{
	// The XEX loader writes the data segment directly to the ANTIC font.
	show_font(tiles);
	return 0;
}
#endif
#endif
