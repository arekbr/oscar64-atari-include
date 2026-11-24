#include <atari/ccmono.h>
#include <atari/dlist.h>
#include <atari/os.h>

#define DISPLAY_ROW 12
#define ACTOR_X     76

static byte fonts[4096];
#pragma align(fonts, 1024)
static byte screen0[1024];
#pragma align(screen0, 1024)
static byte screen1[1024];
#pragma align(screen1, 1024)

static byte display_list[40];
#pragma align(display_list, 1024)

static const byte background[1000] = {0};
static CCEntry history0[CC_HISTORY_ENTRIES], history1[CC_HISTORY_ENTRIES];
static byte glyphs[256];
static const CCConfig config = {
	fonts, screen0, screen1, background, history0, history1, glyphs
};

static CCState compositor;
static byte image_data[2 * 32];
static CCImage image = {image_data, nullptr};
static const byte diamond[8] = {0x18, 0x3c, 0x7e, 0xff, 0x7e, 0x3c, 0x18, 0};

int main(void)
{
	if (!cc_init(&compositor, &config) ||
	    cc_cache_init(&image, 2) != CC_OK ||
	    cc_mono(&image, diamond, sizeof(diamond), 0) != CC_OK)
		return 1;

	cc_begin(&compositor, 1);
	if (cc_draw(&compositor, 0, &image, ACTOR_X, 8 * DISPLAY_ROW) != CC_OK)
		return 1;

	// Show only logical row 12. It uses bank 0, so CHBASE stays fixed.
	volatile byte * dl = dl_blank(display_list, 116);
	dl = dl_lms(dl, DL_MODE4, screen1 + 40 * DISPLAY_ROW);
	dl = dl_blank(dl, 116);
	dl_jvb(dl, display_list);

	os_chbas = (byte)((word)fonts >> 8);
	os_chact = 0;
	os_color[2] = 14;
	os_color[4] = 0;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);

	for (;;)
		os_wait_frame();
}
