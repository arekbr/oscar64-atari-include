#include <atari/cio80.h>
#include <atari/dlist.h>
#include <atari/input.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

static volatile byte list[208];
#pragma align(list, 256)
static ABitmapDisplayPage page;
static byte font[1024];
static const ABitmapPageMemory memory = {(volatile byte *)0x8010, 40 * 192, list, sizeof(list)};
static const ABitmapScreenMode mode = {DL_MODEF, DMACTL_PF_NORMAL, 40, 192, 24};

int main(void)
{
	const byte * rom_font = (const byte *)0xe000;
	for (word i = 0; i < sizeof(font); i++)
	{
		byte row = rom_font[i] | (rom_font[i] << 1);
		font[i] = ((row & 0x80) >> 4) | ((row & 0x20) >> 3) |
			((row & 0x08) >> 2) | ((row & 0x02) >> 1);
	}
	byte ch = cio_free();
	if (ch == 255 || !abm_page_init(&page, &memory, &mode) || cio80_install(&page, font) != CIO_OK)
		return 1;
	if (cio_open(ch, "80:", CIO_UPDATE, 0) != CIO_OK)
	{
		cio80_remove();
		return 2;
	}
	for (byte x = 0; x < 80; x++)
		cio_putc(ch, '0' + x % 10);
	static const char message[] = "CIO DEVICE 80:     PRESS START TO CLOSE";
	word done;
	cio_putrec(ch, message, sizeof(message) - 1, &done);
	while (!(cons_keys() & CONSOL_START))
		os_wait_frame();
	cio_close(ch);
	cio80_remove();
	return 0;
}
