#include "graphics_text40.h"
#include "dlist.h"
#include "gtia.h"
#include "irq.h"
#include "os.h"

static volatile byte font_page;
static byte old_chbas, old_dma, old_priority;
static word old_list, old_screen;
static bool active;

__asm text40_interrupt
{
	pha
	sta $d40a
	lda #5
	sta $d405
	eor #7
	sta text40_interrupt + 5
	lda #0
	sta $d409
	eor #4
	sta text40_interrupt + 15
	pla
	rti
}

__asm text40_vbi
{
	lda #2
	sta $d405
	lda #5
	sta text40_interrupt + 5
	lda font_page
	sta $d409
	eor #4
	sta text40_interrupt + 15
	rts
}

struct Interval
{
	word start, end;
};

static bool interval(Interval * span, word start, word size)
{
	if (!start || start > 0xffffu - (size - 1))
		return false;
	span->start = start;
	span->end = start + (size - 1);
	return true;
}

static bool layout(const AGraphicsText40 * screen)
{
	byte rows = screen->rows;
	if (rows < 2 || (rows & 1) || (word)rows * 6 + screen->top > 240 ||
		((word)screen->fonts & 2047u))
		return false;
	word bytes = (word)rows * 40;
	word length = ((word)screen->top + 7) / 8 + rows + 3;
	word addr = (word)screen->text;
	for (byte row = 0; row < rows; row++, addr += 40)
	{
		word offset = addr & 4095u;
		if (offset > 4096u - 40)
			return false;
		if (!row || offset < 40)
			length += 2;
	}
	if (screen->text_size < bytes || screen->list_size < length ||
		length > 1024u - ((word)screen->list & 1023u))
		return false;

	Interval ranges[5];
	if (!interval(ranges, (word)screen->list, length) ||
		!interval(ranges + 1, (word)screen->text, bytes) ||
		!interval(ranges + 2, (word)screen->fonts, 2048u) ||
		!interval(ranges + 3, (word)screen->font, 1024u) ||
		!interval(ranges + 4, (word)screen, sizeof(AGraphicsText40)))
		return false;
	for (byte i = 1; i < 5; i++)
		for (byte j = 0; j < i; j++)
			if (ranges[i].start <= ranges[j].end && ranges[j].start <= ranges[i].end)
				return false;
	return true;
}

static void build(const AGraphicsText40 * screen)
{
	volatile byte * list = dl_blank(screen->list, screen->top);
	word addr = (word)screen->text;
	for (byte row = 0; row < screen->rows; row++, addr += 40)
	{
		byte flags = DL_MODE2 | (row & 1 ? 0 : DL_VSCROL);
		if (row + 1 < screen->rows)
			flags |= DL_DLI;
		if (!row || (addr & 4095u) < 40)
			list = dl_lms(list, flags, (const volatile void *)addr);
		else
			*list++ = flags;
	}
	dl_jvb(list, screen->list);
	for (word i = 0; i < 1024; i += 8)
	{
		for (byte y = 0; y < 6; y++)
		{
			byte value = screen->font[i + y];
			screen->fonts[i + 2 + y] = value;
			screen->fonts[1024 + i + y] = value;
		}
	}
}

bool gr_text40_show(const AGraphicsText40 * screen)
{
	if (!screen || !layout(screen))
		return false;
	os_wait_frame();
	dli_off();
	vbi_off();
	if (!active)
	{
		old_chbas = os_chbas;
		old_dma = os_sdmctl;
		old_list = os_sdlst;
		old_screen = os_savmsc;
		old_priority = os_gprior;
	}
	os_sdmctl &= (byte)~DMACTL_DLIST;
	os_wait_frame();
	build(screen);
	font_page = (word)screen->fonts >> 8;
	os_savmsc = (word)screen->text;
	os_chbas = font_page;
	os_gprior &= 0x3f;
	os_sdlst = (word)screen->list;
	os_sdmctl = (old_dma & ~3) | DMACTL_PF_NORMAL | DMACTL_DLIST;
	vbi_set((IRQHandler)text40_vbi);
	os_wait_frame();
	dli_set((IRQHandler)text40_interrupt);
	active = true;
	return true;
}

void gr_text40_off(void)
{
	if (!active)
		return;
	os_wait_frame();
	dli_off();
	vbi_off();
	antic.vscrol = 0;
	os_savmsc = old_screen;
	os_sdlst = old_list;
	os_chbas = old_chbas;
	os_gprior = old_priority;
	os_sdmctl = old_dma;
	active = false;
}
