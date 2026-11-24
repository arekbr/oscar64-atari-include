#include <atari/console.h>
#include <atari/xmem.h>

// Keep the code that runs with banked RAM visible outside the window.
#pragma region(xmem, 0x2080, 0x2180, , , {xmemcode})
#pragma region(main, 0x2180, 0xbc00, , , {code, data, bss, heap, stack})

#define BANK 2
#define OFFSET 0x01f0

static byte pattern(byte i)
{
	return (byte)((3 * i) ^ 0xa5);
}

static const char * verify_transfer(void)
{
	static byte data[32], saved[32];

	if (xmem_detect() != XMEM_OK)
		return "Extended memory probe failed";
	if (xmem_banks <= BANK)
		return "No extended memory";
	if (xmem_get(BANK, OFFSET, saved, sizeof(saved)) != XMEM_OK)
		return "Bank backup read failed";

	for (byte i = 0; i < sizeof(data); ++i)
		data[i] = pattern(i);
	if (xmem_put(BANK, OFFSET, data, sizeof(data)) != XMEM_OK)
		return "Bank write failed";

	for (byte i = 0; i < sizeof(data); ++i)
		data[i] = 0;
	const char * result = "Bank read failed";
	if (xmem_get(BANK, OFFSET, data, sizeof(data)) == XMEM_OK)
	{
		result = "Bank transfer verified";
		for (byte i = 0; i < sizeof(data); ++i)
			if (data[i] != pattern(i))
			{
				result = "Bank data mismatch";
				break;
			}
	}

	if (xmem_put(BANK, OFFSET, saved, sizeof(saved)) != XMEM_OK)
		return "Bank restore failed";
	return result;
}

int main(void)
{
	con_clrscr();
	con_printf("%s\nPress RESET to exit\n", verify_transfer());
	return 0;
}
