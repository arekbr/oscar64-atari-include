#include <atari/dlist.h>
#include <atari/gscreen.h>
#include <atari/os.h>

#define POOL_FIRST 64
#define BLANK      0
#define BLOCK      1
#define DIAMOND    GS_DYN

byte codes[GS_CELLS];
#pragma align(codes, 256)

const byte gs_glyphs[2048] = {
	[256 * 0 + DIAMOND] = 0x18,
	[256 * 1 + DIAMOND] = 0x3c,
	[256 * 2 + DIAMOND] = 0x7e,
	[256 * 3 + DIAMOND] = 0xff,
	[256 * 4 + DIAMOND] = 0xff,
	[256 * 5 + DIAMOND] = 0x7e,
	[256 * 6 + DIAMOND] = 0x3c,
	[256 * 7 + DIAMOND] = 0x18
};

static const byte slot_of[GS_DYN] = {0, 1};
static const byte fixed[8 * POOL_FIRST] = {
	// Leave slot 0 blank and put a solid square in slot 1.
	0, 0, 0, 0, 0, 0, 0, 0,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const struct GScreen screen = {
	GS_DYN, (40 - GS_W) / 2, (24 - GS_H) / 2, slot_of, fixed,
	POOL_FIRST, BLANK, nullptr, nullptr, 0
};

static byte display_list[32];
#pragma align(display_list, 1024)

int main(void)
{
	if (gs_init(&screen) != GS_W_NOLIST)
		return 1;

	// Every diamond cell uses the same cached glyph slot.
	for (byte row = 0; row < GS_H; row++)
		for (byte col = 0; col < GS_W; col++)
			codes[row * GS_W + col] = ((row + col) & 1) ? DIAMOND : BLOCK;
	gs_blit();

	volatile byte * dl = dl_blank(display_list, 24);
	dl = dl_lms(dl, DL_MODE2, gs_mem);
	dl = dl_mode(dl, DL_MODE2, 23);
	dl_jvb(dl, display_list);

	os_chbas = (byte)((word)gs_font >> 8);
	os_chact = 0;
	os_color[1] = 14;
	os_color[2] = 0;
	os_color[4] = 0;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);

	for (;;)
		os_wait_frame();
}
