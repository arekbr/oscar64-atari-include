#include <oscar.h>
#include "charset.h"

static const char compressed[] = {
	#embed lzo "tiles.fnt"
};
static byte font[FONT_SIZE];
#pragma align(font, 1024)

int main(void)
{
	oscar_expand_lzo((char *)font, compressed);
	show_font(font);
	return 0;
}
