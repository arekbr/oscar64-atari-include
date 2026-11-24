#include <atari/bitmap_screen.h>
#include <atari/dlist.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x6800, , , {code, data, bss, heap, stack})

#define STATUS (*(volatile byte *)0x0600)

static ABitmapDisplayPage pages[3];
static const ABitmapPageMemory memory[] =
{
	{(volatile byte *)0x8000, 1920, (volatile byte *)0x7000, 128},
	{(volatile byte *)0x9000, 2048, (volatile byte *)0x7302, 128},
	{(volatile byte *)0xa000, 1536, (volatile byte *)0x7500, 128}
};
static const ABitmapScreenMode modes[] =
{
	{DL_MODEF, DMACTL_PF_NORMAL, 40, 48, 24},
	{DL_MODEE, DMACTL_PF_NARROW, 32, 64, 24},
	{DL_MODEF, DMACTL_PF_WIDE, 48, 32, 24}
};

static bool rejected_page(void)
{
	ABitmapPageMemory invalid = memory[1];
	volatile byte * list = invalid.list;
	list[0] = 0x9e;
	invalid.list_size = 2;
	pages[1].bitmap.width = 77;
	if (abm_page_init(pages + 1, &invalid, modes + 1) ||
		pages[1].bitmap.width != 77 || list[0] != 0x9e)
		return false;
	invalid = memory[1];
	invalid.list = (volatile byte *)(pages + 1);
	return !abm_page_init(pages + 1, &invalid, modes + 1) && pages[1].bitmap.width == 77;
}

static bool visible(const ABitmapDisplayPage * page)
{
	return os_sdlst == (word)page->list && (os_sdmctl & 3) == page->playfield;
}

int main(void)
{
	STATUS = 1;
	if (!rejected_page())
		return 1;
	for (byte i = 0; i < 3; i++)
		if (!abm_page_init(pages + i, memory + i, modes + i))
			return 2;
	os_gprior &= 0x3f;
	os_color[0] = 0x0e;
	os_color[1] = 0x06;
	os_color[2] = 0;
	os_color[4] = 0;
	abm_clear(&pages[0].bitmap, 1);
	abm_clear(&pages[1].bitmap, 0);
	abm_clear(&pages[2].bitmap, 0);

	STATUS = 2;
	const ABitmap * draw = &pages[1].bitmap;
	abm_page_show(pages);
	if (!visible(pages))
		return 3;
	abm_fill_rect(draw, 0, 0, draw->width, 16, 1);
	abm_page_show(pages + 2);
	if (!visible(pages + 2))
		return 4;
	abm_fill_rect(draw, 0, 32, draw->width, 16, 2);
	if (abm_get(draw, 127, 15) != 1 || abm_get(draw, 127, 47) != 2 ||
		abm_get(&pages[0].bitmap, 127, 47) != 1 ||
		abm_get(&pages[2].bitmap, 127, 15) != 0)
		return 5;

	abm_page_show(pages + 1);
	if (!visible(pages + 1))
		return 6;
	abm_fill_rect(draw, 0, 56, draw->width, 8, 1);
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
