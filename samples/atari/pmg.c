#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/os.h>
#include <atari/pmg.h>

#ifdef PMG_DOUBLE_LINES
#define RESOLUTION  PMG_2LINE
#define FIRST_ROW   60
#define ROWS        8
static byte pm[1024];
#pragma align(pm, 1024)
#else
#define RESOLUTION  PMG_1LINE
#define FIRST_ROW   120
#define ROWS        16
static byte pm[2048];
#pragma align(pm, 2048)
#endif

static byte display_list[30 + 3];  // Thirty 8-line blanks and a JVB.
#pragma align(display_list, 1024)

int main(void)
{
	// A blank display list gives the player a plain background.
	volatile byte * end = dl_blank(display_list, 240);
	dl_jvb(end, display_list);
	dl_set(display_list);

	os_color[4] = COLOR_BLACK;
	os_gprior = PRIOR_P0123_PF0123;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;

	pmg_init(pm, RESOLUTION);
	volatile byte * player = pmg_player(0);
	for (byte row = 0; row < ROWS; row++)
		player[FIRST_ROW + row] = 0xff;

	gtia.vdelay = 0;
	gtia.sizep[0] = 0;     // One color clock per set bit.
	gtia.hposp[0] = 120;   // Horizontal position in color clocks.
	os_pcolr[0] = COLOR_WHITE;

	for (;;)
		os_wait_frame();
}
