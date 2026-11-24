#include "bitmap_screen.h"
#include "dlist.h"
#include "os.h"

struct ScreenRange
{
	word first, last;
};

static bool overlap(const ScreenRange * ranges, byte count)
{
	for (byte i = 1; i < count; i++)
		for (byte j = 0; j < i; j++)
			if (ranges[i].first <= ranges[j].last && ranges[j].first <= ranges[i].last)
				return true;
	return false;
}

static word prepare(ABitmap * bitmap, const ABitmapPageMemory * memory,
	const ABitmapScreenMode * mode, ScreenRange * ranges)
{
	if (!abm_init_width(bitmap, memory->pixels, memory->pixel_size, mode->pitch,
		mode->height, mode->mode, mode->playfield))
		return 0;
	word length = abm_display_list_size(memory->list, memory->list_size, bitmap, mode->mode, mode->top);
	if (!length)
		return 0;
	ranges[0].first = (word)bitmap->data;
	ranges[0].last = (word)abm_row(bitmap, bitmap->height - 1) + ((bitmap->width * bitmap->bits) >> 3) - 1;
	ranges[1].first = (word)memory->list;
	ranges[1].last = (word)memory->list + (length - 1);
	return length;
}

bool abm_page_init(ABitmapDisplayPage * page, const ABitmapPageMemory * memory, const ABitmapScreenMode * mode)
{
	if (!page || !memory || !mode || (word)page > 0xffffu - (sizeof(ABitmapDisplayPage) - 1))
		return false;
	ABitmapDisplayPage setup;
	setup.list = memory->list;
	setup.playfield = mode->playfield;
	ScreenRange ranges[3];
	ranges[0].first = (word)page;
	ranges[0].last = (word)page + (sizeof(ABitmapDisplayPage) - 1);
	word length = prepare(&setup.bitmap, memory, mode, ranges + 1);
	if (!length || overlap(ranges, 3))
		return false;
	abm_display_list(setup.list, length, &setup.bitmap, mode->mode, mode->top);
	*page = setup;
	return true;
}

bool abm_screen_init(ABitmapScreen * screen, const ABitmapPageMemory * memory, const ABitmapScreenMode * mode)
{
	if (!screen || !memory || !mode || (word)screen > 0xffffu - (sizeof(ABitmapScreen) - 1))
		return false;

	ABitmapScreen setup;
	ScreenRange ranges[5];
	ranges[0].first = (word)screen;
	ranges[0].last = (word)screen + (sizeof(ABitmapScreen) - 1);
	byte bitmap_mode = mode->mode, top = mode->top;
	setup.front = 0;
	setup.playfield = mode->playfield;
	for (byte i = 0; i < 2; i++)
	{
		ABitmapPageMemory page = memory[i];
		if (!prepare(&setup.pages[i], &page, mode, ranges + 1 + 2 * i))
			return false;
		setup.lists[i] = page.list;
	}
	if (overlap(ranges, 5))
		return false;

	for (byte i = 0; i < 2; i++)
	{
		word length = ranges[2 + 2 * i].last - ranges[2 + 2 * i].first + 1;
		abm_display_list(setup.lists[i], length, &setup.pages[i], bitmap_mode, top);
	}
	*screen = setup;
	return true;
}

const ABitmap * abm_screen_back(const ABitmapScreen * screen)
{
	return &screen->pages[screen->front ^ 1];
}

static void show_page(const volatile byte * list, byte playfield)
{
	os_wait_frame();
	os_sdmctl = (os_sdmctl & ~3) | DMACTL_DLIST | playfield;
	os_sdlst = (word)list;
	os_wait_frame();
}

void abm_page_show(const ABitmapDisplayPage * page)
{
	show_page(page->list, page->playfield);
}

void abm_screen_show(const ABitmapScreen * screen)
{
	show_page(screen->lists[screen->front], screen->playfield);
}

void abm_screen_present(ABitmapScreen * screen)
{
	word front = (word)screen->lists[screen->front];
	word back = (word)screen->lists[screen->front ^ 1];
	volatile byte * shadow = (volatile byte *)0x0230;
	screen->front ^= 1;
	if (shadow[0] == (byte)front && shadow[1] == (byte)(front >> 8) &&
		(os_sdmctl & (DMACTL_DLIST | 3)) == (DMACTL_DLIST | screen->playfield))
	{
		// A one-byte shadow update cannot be torn by the OS VBI.
		if ((byte)front == (byte)back)
			shadow[1] = (byte)(back >> 8);
		else if ((byte)(front >> 8) == (byte)(back >> 8))
			shadow[0] = (byte)back;
		else
		{
			abm_screen_show(screen);
			return;
		}
		os_wait_frame();
	}
	else
		abm_screen_show(screen);
}
