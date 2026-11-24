#include "crawler.h"

int main(void)
{
	show_crawler();
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		advance_scroll();
	}
	return 0;
}
