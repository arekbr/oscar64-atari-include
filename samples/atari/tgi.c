#include <tgi.h>
#include <atari/input.h>

#pragma stacksize(1024)
#pragma region(main, 0x2800, 0x7e00, , , {code, data, bss, heap, stack})

#ifdef TGI_EMBED_DRIVER
static const unsigned char atr8[] = {
	#embed "ATR8.TGI"
};
const ATgiModule tgi_static_stddrv[] = {{atr8, sizeof(atr8)}};
#endif

int main(void)
{
#ifdef TGI_EMBED_DRIVER
	tgi_install(tgi_static_stddrv);
#else
	tgi_load_driver(tgi_stddrv);
#endif
	unsigned char status = tgi_geterror();
	if (status)
		return status;
	tgi_init();
	status = tgi_geterror();
	if (!status)
	{
		tgi_clear();
		tgi_gotoxy(28, 142);
		tgi_lineto(84, 45);
		tgi_lineto(142, 142);
		tgi_bar(52, 104, 118, 108);
		tgi_ellipse(214, 91, 50, 36);
		tgi_pieslice(214, 91, 36, 25, 0, 90);
		tgi_outtextxy(88, 169, "OSCAR64  ATARI  TGI");
		while (!(cons_keys() & CONSOL_START))
			os_wait_frame();
	}
	tgi_unload();
	return status;
}
