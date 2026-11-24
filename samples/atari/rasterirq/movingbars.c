#include <atari/colors.h>
#include <atari/input.h>
#include <atari/irq.h>
#include <math.h>
#include "../support/bitmap_demo.h"

#define TOP 8
#define HEIGHT 240
#define BAR_HEIGHT 16

static byte sine[32];
static volatile byte inks[4], ink_index;
static byte lists[2][64];
#pragma align(lists, 1024)

__asm bar_color
{
		pha
		txa
		pha
		ldx ink_index
		lda inks,x
		inc ink_index
		sta $d40a
		sta $d01a
		pla
		tax
		pla
		rti
}

// Schedule only the edges that change color. Red has priority at overlap.
static void build_bars(byte * list, byte red, byte yellow, byte * colors)
{
	volatile byte * end = list;
	byte last = TOP, color = COLOR_BLUE, count = 0;
	for (byte y = TOP + 1; y < HEIGHT; ++y)
	{
		byte next = (byte)(y - red) < BAR_HEIGHT ? COLOR_RED :
		            (byte)(y - yellow) < BAR_HEIGHT ? COLOR_YELLOW : COLOR_BLUE;
		if (next != color)
		{
			end = dl_blank(end, y - last - 1);
			*end++ = DL_BLANK(1) | DL_DLI;
			colors[count++] = next;
			last = y;
			color = next;
		}
	}
	dl_jvb(dl_blank(end, HEIGHT - last), list);
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	void * saved_dli = os_vdslst;
	for (byte i = 0; i < 32; ++i)
		sine[i] = ((int)(120 + 60 * sin(i * PI / 16)) | 1) - 18;

	os_color[4] = COLOR_BLUE;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	byte index = 0, page = 0;
	byte colors[4] = {0, 0, 0, 0};
	build_bars(lists[0], sine[3], sine[0], colors);
	for (byte i = 0; i < 4; ++i)
		inks[i] = colors[i];
	dl_set(lists[0]);
	os_wait_frame();
	dli_set(bar_color);

	while (!(cons_keys() & CONSOL_START))
	{
		page ^= 1;
		index = (index + 1) & 31;
		build_bars(lists[page], sine[(index + 3) & 31], sine[index], colors);
		// Publish below the old bars and before the next OS vertical blank.
		while (antic.vcount < 112)
			;
		os_sdlst = (word)lists[page];
		for (byte i = 0; i < 4; ++i)
			inks[i] = colors[i];
		ink_index = 0;
		os_wait_frame();
	}

	dli_off();
	os_vdslst = saved_dli;
	bitmap_demo_restore(&saved);
	return 0;
}
