#include <atari/hcm.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0xbc00, , , {code, data, bss, heap, stack})
#pragma heapsize(0)
#pragma stacksize(1024)

#ifndef HCM_MODE
#define HCM_MODE DL_MODEE
#endif

static volatile byte players[2048];
#pragma align(players, 2048)
static volatile byte display_list[256];
#pragma align(display_list, 1024)
static volatile byte pixels[6400];
#pragma align(pixels, 32)
#ifdef HCM_SPLIT
static volatile byte raster[HCM_PMG_ROW_PROGRAM_SIZE];
static byte pmg_colors[HCM_ROWS];
#else
static volatile byte raster[HCM_ROW_PROGRAM_SIZE];
#endif
static AHcmRowColor rows[HCM_ROWS];

int main(void)
{
	AHcm screen = {players, display_list, pixels, sizeof(display_list), sizeof(pixels), HCM_MODE};
	AHcmRows program = {rows, raster, sizeof(raster)};
	os_color[4] = 0;
	hcm_colors(0x46, 0xb6);
	for (byte row = 0; row < HCM_ROWS; row++)
	{
		rows[row].reg = (row & 1) ? 0x13 : 0x12;
		rows[row].value = (row & 2) ? 0x8c : 0x3a;
#ifdef HCM_SPLIT
		rows[row].reg = 0x12;
		rows[row].value = 0x36;
		pmg_colors[row] = (row & 1) ? 0xca : 0x9a;
#endif
	}
#ifdef HCM_SPLIT
	program.capacity--;
	if (hcm_show_pmg_rows(&screen, &program, pmg_colors))
		return 3;
	program.capacity++;
	if (!hcm_show_pmg_rows(&screen, &program, pmg_colors))
#else
	if (!hcm_show_rows(&screen, &program))
#endif
		return 1;
	for (byte y = 0; y < HCM_ROWS; y++)
		for (byte x = 0; x < HCM_COLUMNS; x++)
			#ifdef HCM_SPLIT
			if (!hcm_color(&screen, x, y, 2))
#else
			if (!hcm_color(&screen, x, y, (x + y) & 3))
#endif
				return 2;
	*(volatile byte *)0x0600 = 0xa5;
	for (;;)
		os_wait_frame();
}
