#include <atari/antic.h>
#include <atari/console.h>
#include <atari/os.h>
#include <atari/romram.h>

#pragma region(main, 0x2080, 0xbc00, , , {code, data, bss, heap, stack})

static const char * check_ram(void)
{
	if (!rr_install(NMIEN_VBI))
		return "RAM under ROM setup failed";

	byte token = rr_open();
	byte original = RR_LOW[0];
	byte probe = original ^ 0xff;
	RR_LOW[0] = probe;
	bool matched = RR_LOW[0] == probe;
	RR_LOW[0] = original;
	bool restored = RR_LOW[0] == original;
	rr_close(token);

	if (!rr_uninstall(NMIEN_VBI))
		return "RAM interrupt restore failed";
	if (!restored)
		return "RAM byte restore failed";
	if (!matched)
		return "RAM test failed";
	return "RAM under ROM verified";
}

int main(void)
{
	con_clrscr();

	// Stop ANTIC fetching the OS font while its ROM is hidden.
	byte dma = os_sdmctl;
	os_sdmctl = 0;
	antic.dmactl = 0;
	const char * result = check_ram();
	os_sdmctl = dma;
	antic.dmactl = dma;

	con_printf("%s\nPress RESET to exit\n", result);
	return 0;
}
