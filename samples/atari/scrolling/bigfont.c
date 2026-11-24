#include <atari/irq.h>
#include "columns.h"
#include "../support/c64_palette.h"

#define TEXT_ROW 5
#define TOP_SCANLINE 32

static const char text[] =
	"Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt "
	"ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
	"dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit "
	"amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor "
	"invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam "
	"et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";
static const byte * const rom_font = (const byte *)0xe000;
static byte font[1024];
#pragma align(font, 1024)

// Set the ink on the scan line before each enlarged glyph row.
__asm next_ink
{
		pha
		txa
		pha
		cld
		lda $d40b
		sec
		sbc #((TOP_SCANLINE + 8 * (TEXT_ROW - 1) + 7) / 2)
		lsr
		lsr
		clc
		adc #1
		and #15
		tax
		sta $d40a
		lda palette,x
		sta $d018
		pla
		tax
		pla
		rti
}

int main(void)
{
	for (byte y = 0; y < 8; ++y)
		font[8 + y] = 0xff;

	ColumnDisplayState saved;
	void * saved_dli = os_vdslst;
	column_display(&saved, DL_MODE4, font, TEXT_ROW - 1, TEXT_ROW + 15);
	os_color[2] = COLOR_WHITE;
	os_color[4] = COLOR_BLACK;
	os_wait_frame();
	dli_set(next_ink);

	word ci = 0;
	byte left = 0, phase = 0, phase_mask = 0x80, repeat = 2;
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		phase = (phase + 1) & 3;
		antic.hscrol = 3 - phase;
		if (phase != 0)
			continue;

		byte ascii = text[ci];
		const byte * glyph = rom_font + 8 * (ascii < 96 ? ascii - 32 : ascii);
		byte x = ++left + FETCH - MARGIN - 1;
		for (byte y = 0; y < 8; ++y)
		{
			byte ink = (glyph[y] & phase_mask) != 0;
			column_put(x, TEXT_ROW + 2 * y, ink);
			column_put(x, TEXT_ROW + 2 * y + 1, ink);
		}
		column_position(left);

		if (--repeat == 0)
		{
			repeat = 2;
			phase_mask >>= 1;
			if (!phase_mask)
			{
				phase_mask = 0x80;
				if (!text[++ci])
					ci = 0;
			}
		}
	}

	dli_off();
	os_vdslst = saved_dli;
	column_close(&saved);
	return 0;
}
