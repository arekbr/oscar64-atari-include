#include <atari/pointer.h>
#include <atari/gr0_cursor.h>
#include <atari/console.h>
#include <atari/input.h>
#include <atari/pokey.h>

#ifndef POINTER_TYPE
#define POINTER_TYPE POINTER_ST
#endif

#ifndef POINTER_PORT
#define POINTER_PORT 0
#endif

static AGr0Cursor cursor;

int main(void)
{
	con_clrscr();
	con_cursor(0);
	con_printf("MOVE THE MOUSE. START EXITS.\n");
	pokey.audctl = 0;
	if (!pointer_irq_start(POINTER_TYPE, POINTER_PORT, 0, 0))
	{
		con_cursor(1);
		return 1;
	}

	gr0_cursor_init(&cursor);
	gr0_cursor_move(&cursor, 160, 96);
	gr0_cursor_show(&cursor, true);
	byte border = os_color[4];
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		APointerMotion motion;
		pointer_irq_take(&motion);
		if (motion.dx || motion.dy || motion.buttons)
			os_atract = 0;
		gr0_cursor_move(&cursor, cursor.x + motion.dx, cursor.y + motion.dy);
		con_border(motion.buttons ? 0x3a : border);
	}

	pointer_irq_stop();
	gr0_cursor_show(&cursor, false);
	con_border(border);
	con_cursor(1);
	return 0;
}
