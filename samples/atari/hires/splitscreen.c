#include <atari/bitmap.h>
#include <atari/colors.h>
#include <atari/console.h>
#include <atari/dlist.h>
#include <atari/os.h>
#include <stdio.h>
#include "../support/display_state.h"

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#define LINES 160
#define TEXT_TOP 19

static ABitmap screen, drawing;
static byte display_list[184];
#pragma align(display_list, 1024)
static char input[40];

int main(void)
{
	if (!abm_init(&screen, (volatile byte *)0x8010, 40 * LINES, 40, LINES, DL_MODEF))
		return 1;
	drawing = screen;
	drawing.height = LINES - 2;
	for (byte y = 0; y < drawing.height; ++y)
	{
		volatile byte * row = abm_row(&screen, y);
		byte pattern = y & 1 ? 0xaa : 0x55;
		for (byte x = 0; x < 40; ++x)
			row[x] = pattern;
	}
	abm_fill_rect(&screen, 0, 158, 320, 1, 0);
	abm_fill_rect(&screen, 0, 159, 320, 1, 1);

	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	con_clrscr();
	volatile byte * end = dl_blank(display_list, 24);
	end = dl_lms(end, DL_MODEF, screen.data);
	end = dl_mode(end, DL_MODEF, 101);
	end = dl_lms(end, DL_MODEF, abm_row(&screen, 102));
	end = dl_mode(end, DL_MODEF, 57);
	end = dl_lms(end, DL_MODE2, (volatile byte *)os_savmsc + TEXT_TOP * 40);
	end = dl_mode(end, DL_MODE2, 4);
	dl_jvb(end, display_list);
	os_color[1] = COLOR_WHITE;
	os_color[2] = os_color[4] = COLOR_BLACK;
	dl_set(display_list);

	con_gotoxy(0, TEXT_TOP);
	con_printf("Enter x, y and radius for circle");
	for (;;)
	{
		con_gotoxy(0, TEXT_TOP + 3);
		con_clreol();
		if (con_gets(input, sizeof(input)) < 0 || input[0] == 'Q' || input[0] == 'q')
			break;

		int x = 0, y = 0, radius = 0;
		int n = sscanf(input, "%d %d %d", &x, &y, &radius);
		con_gotoxy(0, TEXT_TOP + 1);
		con_clreol();
		con_printf("%.36s", input);
		con_gotoxy(0, TEXT_TOP + 2);
		con_clreol();
		con_printf("N: %d X: %3d Y: %3d R: %2d", n, x, y, radius);
		if (n == 3 && radius >= 1 && radius < LINES - 2 && x > -160 && x < 480 && y > -160 && y < 320)
		{
			abm_fill_ellipse(&drawing, x, y, radius + 1, radius + 1, 0);
			abm_fill_ellipse(&drawing, x, y, radius - 1, radius - 1, 1);
		}
	}
	bitmap_demo_restore(&saved);
	return 0;
}
