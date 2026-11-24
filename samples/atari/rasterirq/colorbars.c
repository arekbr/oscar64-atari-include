#include <atari/input.h>
#include <atari/irq.h>
#include "../support/bitmap_demo.h"
#include "../support/c64_palette.h"

#define FIRST_LINE 62
#define BARS 15

__asm bar_color
{
		pha
		txa
		pha
		cld
		lda $d40b
		sec
		sbc #((FIRST_LINE - 1) / 2)
		lsr
		lsr
		and #15
		tax
		sta $d40a
		lda palette,x
		sta $d01a
		pla
		tax
		pla
		rti
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	void * saved_dli = os_vdslst;

	volatile byte * end = dl_blank(display_list, FIRST_LINE - 9);
	*end++ = DL_BLANK(1) | DL_DLI;
	for (byte i = 1; i < BARS; ++i)
		*end++ = DL_BLANK8 | DL_DLI;
	dl_jvb(dl_blank(end, 56), display_list);
	os_color[4] = COLOR_LIGHTBLUE;
	dl_set(display_list);
	os_wait_frame();
	dli_set(bar_color);

	while (!(cons_keys() & CONSOL_START))
		os_wait_frame();

	dli_off();
	os_vdslst = saved_dli;
	bitmap_demo_restore(&saved);
	return 0;
}
