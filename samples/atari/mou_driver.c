#include <atari/mou_driver.h>
#include <atari/gr0_cursor.h>
#include <atari/input.h>
#include <atari/console.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#ifndef MOU_FILE
#define MOU_FILE "D:ATRST.MOU"
#endif

static byte file[1200], image[800];
static const AMouStorage storage = {file, image, sizeof(file), sizeof(image)};
static const AMouSound sound = {0x28, 0, 0};
static AMouDriver driver;
static AGr0Cursor cursor;

int main(void)
{
	if (!amou_load_file(&driver, MOU_FILE, &storage) || !amou_open(&driver, &sound))
		return 1;

	con_clrscr();
	con_cursor(0);
	con_printf("CC65 MOUSE MODULE. START EXITS.\n");
	gr0_cursor_init(&cursor);
	gr0_cursor_show(&cursor, true);
	byte border = os_color[4];
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		AMouInfo info;
		if (amou_poll() && amou_info(&info))
		{
			gr0_cursor_move(&cursor, info.x, info.y);
			con_border(info.buttons ? 0x3a : border);
		}
	}
	amou_close();
	gr0_cursor_show(&cursor, false);
	con_border(border);
	con_cursor(1);
	return 0;
}
