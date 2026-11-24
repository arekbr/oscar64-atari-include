#include <atari/pmg_pointer.h>
#include <atari/os.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

#ifndef POINTER_TYPE
#define POINTER_TYPE PMG_POINTER_ST
#endif

static const byte arrow[] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xe0, 0xb0, 0x18};
static const APmgCursorConfig box = {arrow, sizeof(arrow), 0, 0, 2, 48, 32, 195, 212};
static APmgCursor cursor;
static APmgPointer pointer;

int main(void)
{
	pmg_init((volatile byte *)0x8000, PMG_1LINE);
	if (!pmg_cursor_init(&cursor, &box) || !pmg_cursor_move(&cursor, 120, 96) ||
		!pmg_cursor_show(&cursor, true) || !pmg_pointer_init(&pointer, &cursor, POINTER_TYPE, 0))
		return 1;
	os_gprior = PRIOR_P0123_PF0123;
	os_pcolr[2] = 14;
	byte frame = os_rtclok[2];
	for (;;)
	{
		if (POINTER_TYPE != PMG_POINTER_TABLET)
			pmg_pointer_poll(&pointer);
		byte tick = os_rtclok[2];
		if (tick != frame)
		{
			frame = tick;
			pmg_pointer_update(&pointer);
			os_pcolr[2] = pointer.buttons ? 0x3a : 14;
		}
	}
}
