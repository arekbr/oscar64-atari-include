#include <atari/charwin.h>
#include <atari/console.h>
#include <atari/os.h>

int main(void)
{
	CharWin screen, status, messages;

	con_cursor(false);
	con_clrscr();
	cwin_init_screen(&screen, (volatile byte *)os_savmsc, CWIN_COLS, CWIN_ROWS);
	cwin_init_sub(&status, &screen, 2, 3, 15, 2);
	cwin_init_sub(&messages, &screen, 20, 3, 18, 2);

	cwin_putat_string(&status, 0, 0, "Status");
	cwin_putat_string(&status, 0, 1, "Ready");

	// Three lines in a two-row window scroll the first line out.
	cwin_console_write_string(&messages, "Starting\nChecking\nDone");

	for (;;)
		os_wait_frame();
	return 0;
}
