#include <atari/dlist.h>
#include <atari/input.h>
#include "../support/bear.h"

static byte list[40];
#pragma align(list, 1024)

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte colors[4];
	for (byte i = 0; i < 4; ++i) colors[i] = os_pcolr[i];

	// A blank ANTIC instruction still fetches player/missile graphics.
	// Reserve visible blank space beyond the bottom of the text playfield.
	volatile byte * end = dl_blank(list, 24);
	end = dl_lms(end, DL_MODE2, (const void *)os_savmsc);
	end = dl_mode(end, DL_MODE2, 21);
	dl_jvb(dl_blank(end, 40), list);
	bear_init();
	gtia.sizem = 0x55;
	os_sdmctl = (os_sdmctl & ~DMACTL_PF_WIDE) | DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(list);

	byte phase = 0;
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		for (byte i = 0; i < 4; ++i)
			bear_move(i, 100 + 40 * i, (byte)(phase * (i + 1)), true);
		if (++phase == 255)
			phase = 0;
	}

	pmg_off();
	for (byte i = 0; i < 4; ++i) os_pcolr[i] = colors[i];
	bitmap_demo_restore(&saved);
	return 0;
}
