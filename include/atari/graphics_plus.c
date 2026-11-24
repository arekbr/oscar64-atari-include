#include "graphics_plus.h"
#include "dlist.h"
#include "irq.h"
#include "gtia.h"
#include "os.h"

static byte active_mode, active_rows, active_top;

__asm plus_interrupt
{
	pha
	sta $d40a
	lda #0
	sta $d405
	lda #0
	sta $d405
	pla
	rti
}

static bool height_valid(byte mode, byte rows, byte top, byte height)
{
	byte maximum = mode == GR_PLUS_GTIA10 ? 16 : 6;
	word extra = mode == GR_PLUS_GTIA10 ? 1 : 0;
	return height >= 2 && height <= maximum && (word)rows * height + top + extra <= 240;
}

static void row_height(byte mode, byte height)
{
	// Both writes are sampled at the start of the next scan line. Use
	// immediate loads and patch their value bytes during vertical blank.
	byte upper = (mode == GR_PLUS_GTIA10 ? 17 : 8) - height;
	byte lower = height - 1;
	__asm {
		lda upper
		sta plus_interrupt + 5
		lda lower
		sta plus_interrupt + 10
	}
}

static word list_length(const AGraphicsPlus * screen)
{
	byte rows = screen->rows, mode = screen->mode, top = screen->top;
	if ((mode != GR_PLUS_TEXT2 && mode != GR_PLUS_TEXT4 && mode != GR_PLUS_GTIA10) ||
		rows < 2 || (rows & 1) || !height_valid(mode, rows, top, screen->row_height))
		return 0;
	word data = (word)screen->pixels, list = (word)screen->list;
	word extent = (word)rows * 40;
	if (!data || !list || screen->pixel_size < extent || data > 0xffffu - (extent - 1))
		return 0;
	word length = ((word)top + 7) / 8 + rows + 3;
	if (mode == GR_PLUS_GTIA10)
		length++;
	word addr = data;
	for (byte row = 0; row < rows; row++, addr += 40)
	{
		word offset = addr & 0x0fffu;
		if (offset > 4096u - 40)
			return 0;
		if (!row || offset < 40)
			length += 2;
	}
	word descriptor = (word)screen;
	if (descriptor > 0xffffu - (sizeof(AGraphicsPlus) - 1) ||
		(list <= descriptor + (sizeof(AGraphicsPlus) - 1) && descriptor <= list + (length - 1)))
		return 0;
	if (length > screen->list_size || length > 1024u - (list & 1023u) ||
		(list <= data + (extent - 1) && data <= list + (length - 1)))
		return 0;
	return length;
}

static void build(const AGraphicsPlus * screen)
{
	byte mode = screen->mode;
	volatile byte * list = dl_blank(screen->list, screen->top);
	if (mode == GR_PLUS_GTIA10)
		*list++ = DL_DLI;

	word addr = (word)screen->pixels;
	for (byte row = 0; row < screen->rows; row++, addr += 40)
	{
		byte instruction;
		if (mode == GR_PLUS_GTIA10)
			instruction = DL_MODEF | (row & 1 ? DL_DLI : DL_VSCROL);
		else if (row & 1)
			instruction = mode | (row + 1 < screen->rows ? DL_DLI : 0);
		else
			instruction = mode | DL_VSCROL | (row ? 0 : DL_DLI);

		if (!row || (addr & 0x0fff) < 40)
			list = dl_lms(list, instruction, (const volatile void *)addr);
		else
			*list++ = instruction;
	}
	dl_jvb(list, screen->list);
}

bool gr_plus_show(const AGraphicsPlus * screen)
{
	if (!screen || !list_length(screen))
		return false;
	byte mode = screen->mode;
	os_wait_frame();
	dli_off();
	os_sdmctl &= (byte)~DMACTL_DLIST;
	os_wait_frame();
	build(screen);
	row_height(mode, screen->row_height);
	active_mode = mode;
	active_rows = screen->rows;
	active_top = screen->top;
	os_savmsc = (word)screen->pixels;
	os_sdmctl = (os_sdmctl & ~3) | DMACTL_DLIST | DMACTL_PF_NORMAL;
	os_gprior = (os_gprior & 0x3f) | (mode == GR_PLUS_GTIA10 ? PRIOR_GTIA_MODE10 : 0);
	os_sdlst = (word)screen->list;
	os_wait_frame();
	dli_set((IRQHandler)plus_interrupt);
	return true;
}

bool gr_plus_height(byte height)
{
	if (!active_mode || !height_valid(active_mode, active_rows, active_top, height))
		return false;
	os_wait_frame();
	row_height(active_mode, height);
	return true;
}

void gr_plus_off(void)
{
	if (!active_mode)
		return;
	os_wait_frame();
	dli_off();
	antic.vscrol = 0;
	active_mode = 0;
}
