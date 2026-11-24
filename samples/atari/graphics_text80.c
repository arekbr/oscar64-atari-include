#include <atari/graphics_text80.h>
#include <atari/charwin.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

// 102 scanlines precede the $9000 boundary. Each scanline fits in one page.
#define PIXELS ((volatile byte *)0x8010)

static volatile byte list[208];
#pragma align(list, 256)
static byte font[1024];

// Each octal digit describes one three-pixel scanline. The fourth pixel
// is a space. This example font covers uppercase letters, digits and '-'.
static const word letters[] = {
	025755, 065656, 034443, 065556, 074647, 074644, 034553, 055755, 072227,
	011152, 055655, 044447, 057755, 057775, 025552, 065644, 025573, 065655,
	034216, 072222, 055557, 055552, 055775, 055255, 055222, 071247
};
static const word digits[] = {
	025552, 032227, 061247, 061216, 055711, 074616, 034652, 071224, 025252, 025316
};

static void glyph(byte ch, word pattern)
{
	byte * rows = font + ((word)cwin_a2s(ch) << 3);
	for (byte y = 5; y; y--)
	{
		rows[y] = (pattern & 7) << 1;
		pattern >>= 3;
	}
}

int main(void)
{
	for (byte i = 0; i < 26; i++)
		glyph('A' + i, letters[i]);
	for (byte i = 0; i < 10; i++)
		glyph('0' + i, digits[i]);
	glyph('-', 00700);

	ABitmap bitmap;
	if (!abm_init(&bitmap, PIXELS, 40 * 192, 40, 192, DL_MODEF) ||
		!abm_display_list(list, sizeof(list), &bitmap, DL_MODEF, 24))
		return 1;
	abm_clear(&bitmap, 0);
	gr_text80_write(&bitmap, 14, 2, "OSCAR64 - 80 COLUMNS ON ATARI XL AND XE", font);
	gr_text80_write(&bitmap, 14, 4, "ANTIC F - 320 BY 192 PIXELS", font);
	char ruler[81];
	for (byte column = 0; column < GR_TEXT80_COLS; column++)
		ruler[column] = '0' + column % 10;
	ruler[80] = 0;
	gr_text80_write(&bitmap, 0, 0, ruler, font);
	gr_text80_write(&bitmap, 0, 23, ruler, font);
	gr_text80_put(&bitmap, 79, 23, '9' | 0x80, font);
	os_color[1] = 0x0e;
	os_color[2] = 0;
	os_color[4] = 0;
	os_gprior &= 0x3f;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(list);
	for (;;)
		os_wait_frame();
}
