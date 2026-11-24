#include "os.h"

void os_wait_frame(void)
{
	byte t = os_rtclok[2];
	while (os_rtclok[2] == t)
		;
}

void os_wait_frames(word n)
{
	while (n--)
		os_wait_frame();
}
