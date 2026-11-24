#include <atari/irq.h>
#include "crawler.h"

static __interrupt void scroll_vbi(void)
{
	advance_scroll();
}

int main(void)
{
	show_crawler();
	vbi_set(scroll_vbi);
	while (!(cons_keys() & CONSOL_START))
		os_wait_frame();
	vbi_off();
	return 0;
}
