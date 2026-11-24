#include <atari/hcm.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#ifndef HCM_MODE
#define HCM_MODE DL_MODEE
#endif

#define STATUS (*(volatile byte *)0x0600)
#define PMG ((volatile byte *)0x8000)
#define PIXELS ((volatile byte *)0x9000)

static volatile byte list[256];
#pragma align(list, 256)
static const AHcm screen = {PMG, list, PIXELS, sizeof(list), 6400, HCM_MODE};

static bool paint(void)
{
	for (byte y = 0; y < HCM_SCANLINES; y++)
		for (byte x = 0; x < HCM_COLUMNS; x++)
			if (!hcm_span(&screen, x, y, 1, (x + y) & 3))
				return false;
	return true;
}

static bool verify(void)
{
	for (byte y = 0; y < HCM_SCANLINES; y++)
		for (byte x = 0; x < HCM_COLUMNS; x++)
			if (hcm_read_scanline(&screen, x, y) != ((x + y) & 3))
				return false;
	for (byte x = 0; x < HCM_COLUMNS; x++)
		if (hcm_read(&screen, x, 24) != (x & 3))
			return false;
	return !hcm_read_scanline(&screen, HCM_COLUMNS, 199) &&
		!hcm_read_scanline(&screen, 27, HCM_SCANLINES) &&
		!hcm_read_scanline(0, 27, 199) && !hcm_read(&screen, 27, HCM_ROWS);
}

int main(void)
{
	STATUS = 1;
	if (!hcm_show(&screen))
		return 1;
	hcm_colors(0x46, 0xb6);
	for (byte i = 0; i < 5; i++)
		os_color[i] = 0;
	for (word i = 0; i < 6400; i++)
		PIXELS[i] = 0;
	STATUS = 2;
	if (!paint())
		return 2;
	STATUS = 3;
	if (!verify())
		return 3;
	STATUS = 0xa5;
	for (;;)
		os_wait_frame();
}
