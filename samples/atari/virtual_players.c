#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/os.h>
#include <atari/vpmg.h>

static byte player_memory[4096];
#pragma align(player_memory, 2048)
static byte display_list[128];
#pragma align(display_list, 1024)
static byte blank_row[40];

static const byte boundary[] = {80};
static const byte pixels[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const byte colors[] = {
	ATARI_COLOR(3, 6), ATARI_COLOR(12, 8), ATARI_COLOR(8, 6), ATARI_COLOR(0, 14)
};

int main(void)
{
	os_color[2] = 0;
	os_color[4] = 0;

	volatile byte * dl = dl_blank(display_list, 24);
	// The seventh character row ends just before scan line 80.
	for (byte row = 0; row < 24; ++row)
		dl = dl_lms(dl, row == 6 ? DL_MODE2 | DL_DLI : DL_MODE2, blank_row);
	dl_jvb(dl, display_list);
	dl_set(display_list);

	vpmg_init(player_memory, player_memory + 2048);
	if (!vpmg_zones(boundary, sizeof(boundary)))
		return 1;

	for (byte slot = 0; slot < 4; ++slot)
	{
		vpmg_set(slot, 60 + slot * 36, 56, pixels, sizeof(pixels), colors[slot]);
		vpmg_set(4 + slot, 72 + slot * 36, 96, pixels, sizeof(pixels), colors[3 - slot]);
	}

	if (vpmg_update() != 0)
		return 1;
	vpmg_start();
	for (;;)
		os_wait_frame();
	return 0;
}
