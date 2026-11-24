#include <atari/tgi_driver.h>
#include <atari/input.h>

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

static byte file[4096], image[3072];
static const ATgiStorage memory = {file, image, sizeof(file), sizeof(image)};
static ATgiDriver driver;

int main(void)
{
	if (!atgi_load_file(&driver, "D:ATR8.TGI", &memory) || !atgi_open(&driver, 0x7800))
		return 1;
	ATgiRequest call = {0};
	call.operation = ATGI_SETCOLOR;
	call.a = 1;
	if (atgi_call(&call))
	{
		atgi_close();
		return 2;
	}
	call.operation = ATGI_LINE;
	for (word x = 0; x < driver.width; x += 16)
	{
		call.p1 = driver.width / 2;
		call.p2 = driver.height / 2;
		call.p3 = x;
		call.p4 = 0;
		atgi_call(&call);
		call.p4 = driver.height - 1;
		atgi_call(&call);
	}
	while (!(cons_keys() & CONSOL_START))
		os_wait_frame();
	atgi_close();
	return 0;
}
