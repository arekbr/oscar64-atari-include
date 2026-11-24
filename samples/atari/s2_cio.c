#include <atari/s2_cio.h>
#include <atari/os.h>

// MyDOS can place the resident S_VBXE code above the default $2000 startup.
#pragma section(startup, 0)
#pragma region(startup, 0x2700, 0x2780, , , {startup})
#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

int main(void)
{
	byte ch = cio_free();
	AS2Mode mode;
	if (s2_cio_open(ch, S2_TEXT_80X32, 0, &mode) != CIO_OK)
		return 1;
	if (mode.width != 80 || mode.height != 32)
		return 2;
	static const char line[] = "OSCAR64 CIO S2: - VBXE TEXT 80 X 32";
	word done;
	if (cio_putrec(ch, line, sizeof(line) - 1, &done) >= 128)
		return 3;
	for (;;)
		os_wait_frame();
}
