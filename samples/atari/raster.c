#include <atari/antic.h>
#include <atari/gtia.h>
#include <atari/os.h>

int main(void)
{
	// The OS copies the background color from its shadow at vertical blank.
	os_color[4] = ATARI_COLOR(3, 4);

	for (;;)
	{
		os_wait_frame();
		while (antic.vcount != 60)
			;
		// VCOUNT counts scanline pairs. Change COLBK at the next scanline.
		antic.wsync = 0;
		gtia.colbk = ATARI_COLOR(12, 8);
	}
}
