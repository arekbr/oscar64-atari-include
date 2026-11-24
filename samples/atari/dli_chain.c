#include <atari/dlic.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>

#define BAND_LINES 60
#define SLOT_COUNT 3

static byte display_list[40];
#pragma align(display_list, 1024)

static DLISlot slots[SLOT_COUNT];
static DLISlot * chain[SLOT_COUNT] = {&slots[0], &slots[1], &slots[2]};
static const byte colors[SLOT_COUNT] = {
	ATARI_COLOR(3, 4), ATARI_COLOR(12, 8), ATARI_COLOR(8, 4)
};

static bool build_chain(void)
{
	volatile byte * dl = display_list;
	for (byte i = 0; i < SLOT_COUNT; i++)
	{
		// Request a DLI on the last scan line of this band.
		dl = dl_blank(dl, BAND_LINES - 8);
		*dl++ = DL_BLANK8 | DL_DLI;
		if (!dlic_begin(chain[i], true) ||
		    dlic_write(chain[i], &gtia.colbk, colors[i]) == DLIC_FULL ||
		    !dlic_end(chain[i]))
			return false;
	}
	dl = dl_blank(dl, BAND_LINES);
	dl_jvb(dl, display_list);
	dlic_chain(chain, SLOT_COUNT);
	return true;
}

int main(void)
{
	if (!build_chain())
		return 1;

	const void * saved_display = (const void *)os_sdlst;
	void * saved_dli = os_vdslst;
	byte saved_background = os_color[4];
	os_color[4] = ATARI_COLOR(0, 0);
	dl_set(display_list);
	dlic_start(chain[0]);

	// START returns to the display that was active on entry.
	while (gtiar.consol & CONSOL_START)
		os_wait_frame();

	dlic_stop();
	os_vdslst = saved_dli;
	os_color[4] = saved_background;
	dl_set(saved_display);
	os_wait_frame();
	return 0;
}
