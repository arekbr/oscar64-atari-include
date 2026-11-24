#include "charset.h"

static const byte tiles[FONT_SIZE] = {
	#embed "tiles.fnt"
};
static volatile byte font[FONT_SIZE];
#pragma align(font, 1024)

int main(void)
{
	for (word i = 0; i < FONT_SIZE; ++i)
		font[i] = tiles[i];
	show_font(font);
	return 0;
}
