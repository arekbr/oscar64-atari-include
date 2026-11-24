#include <atari/antic.h>
#include <atari/console.h>
#include <atari/os.h>
#include <atari/romram.h>

#pragma region(main, 0x2080, 0xbc00, , , {code, data, bss, heap, stack})

#define BLOCK_SIZE 1024u

static bool fill_blocks(volatile byte * block, word length, word * total)
{
	while (length >= BLOCK_SIZE)
	{
		byte token = rr_open();
		for (word i = 0; i < BLOCK_SIZE; ++i)
			block[i] = 0xaa;

		word checked = 0;
		while (checked < BLOCK_SIZE && block[checked] == 0xaa)
			++checked;
		rr_close(token);

		if (checked != BLOCK_SIZE)
			return false;
		*total += BLOCK_SIZE;
		con_printf("ALLOCATED %5u AT %04x\n", *total, (word)block);
		block += BLOCK_SIZE;
		length -= BLOCK_SIZE;
	}
	return true;
}

int main(void)
{
	con_clrscr();
	if (!rr_install(NMIEN_VBI))
	{
		con_printf("RAM under ROM setup failed\n");
		return 1;
	}

	// The console font is in ROM. Stop ANTIC fetching it during the test.
	byte dma = os_sdmctl;
	os_sdmctl = 0;
	antic.dmactl = 0;
	word total = 0;
	bool filled = fill_blocks(RR_LOW, RR_LOW_SIZE, &total) &&
		fill_blocks(RR_HIGH, RR_HIGH_SIZE, &total);
	bool removed = rr_uninstall(NMIEN_VBI);
	os_sdmctl = dma;
	antic.dmactl = dma;

	if (!filled || !removed)
	{
		con_printf("RAM under ROM test failed\n");
		return 1;
	}
	return 0;
}
