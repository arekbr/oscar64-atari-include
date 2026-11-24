#include <atari/os.h>
#include <stdio.h>
#include <stdlib.h>

// With BASIC disabled, the heap can grow through $A000. Leave the
// GRAPHICS 0 allocation at $BC00 and above to the OS.
#pragma region(main, 0x2080, 0xbc00, , , {code, data, bss, heap, stack})

int main(void)
{
	if (os_ramtop < 0xc0)
	{
		printf("DISABLE BASIC BEFORE LOADING THIS XEX.\n");
		return 1;
	}

	word total = 0;
	bool reached_basic_ram = false;
	volatile byte * block;
	while ((block = (volatile byte *)malloc(1024)) != nullptr)
	{
		for (word offset = 0; offset < 1024; ++offset)
			block[offset] = 0xaa;
		total += 1024;
		reached_basic_ram |= (word)block + 1023 >= 0xa000;
		printf("ALLOCATED %u AT $%04X\n", total, (word)block);
	}
	printf("HEAP EXHAUSTED AFTER %u BYTES\n", total);
	if (reached_basic_ram)
		printf("USED RAM AT $A000 AND ABOVE\n");
	return 0;
}
