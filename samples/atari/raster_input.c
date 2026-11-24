#include <atari/dlist.h>
#include <atari/input.h>
#include <atari/irq.h>

#define BAND_LINES	96

static byte display_list[32];
#pragma align(display_list, 1024)

static volatile byte lower_color = ATARI_COLOR(12, 8);

static __interrupt void poll_joystick(void)
{
	lower_color = joy_fire(0) ? ATARI_COLOR(0, 14) : ATARI_COLOR(12, 8);
}

static __hwinterrupt void draw_lower_band(void)
{
	antic.wsync = 0;
	gtia.colbk = lower_color;
}

int main(void)
{
	volatile byte * dl = display_list;

	// The final blank instruction in the upper band requests the DLI.
	dl = dl_blank(dl, BAND_LINES - 8);
	*dl++ = DL_BLANK8 | DL_DLI;
	dl = dl_blank(dl, BAND_LINES);
	dl_jvb(dl, display_list);

	os_color[4] = ATARI_COLOR(3, 4);
	dl_set(display_list);
	vbi_set(poll_joystick);
	dli_set(draw_lower_band);

	for (;;)
		os_wait_frame();
}
