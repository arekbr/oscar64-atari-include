#include <atari/console.h>
#include <atari/graphics.h>
#include <atari/gtia.h>
#include <atari/os.h>

// Leave room for the screen and display list allocated by the OS.
#pragma region(main, 0x2080, 0x8000, , , {code, data, bss, heap, stack})

int main(void)
{
	AGraphics screen;
	if (gr_open(&screen, 6, 7, GR_SPLIT) != CIO_OK)
		return 1;

	os_color[0] = ATARI_COLOR(3, 10);
	os_color[1] = ATARI_COLOR(0, 14);
	os_color[2] = ATARI_COLOR(0, 0);
	os_color[4] = ATARI_COLOR(0, 0);

	if (gr_plot(&screen, 32, 10, 1) != CIO_OK ||
	    gr_draw_to(&screen, 96, 10, 1) != CIO_OK ||
	    gr_draw_to(&screen, 96, 74, 1) != CIO_OK ||
	    gr_draw_to(&screen, 32, 74, 1) != CIO_OK ||
	    gr_draw_to(&screen, 32, 10, 1) != CIO_OK ||
	    gr_draw_to(&screen, 96, 74, 1) != CIO_OK)
		return 1;

	con_printf("GRAPHICS 7 with a text window\n");
	gr_close(&screen);
	return 0;
}
