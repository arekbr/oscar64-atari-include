#include <oscar.h>
#include "overlay_demo.h"

#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})

static const char module_1[] = {
	#embed lzo "ovl1.bin"
};
static const char module_2[] = {
	#embed lzo "ovl2.bin"
};
static const char module_3[] = {
	#embed lzo "ovl3.bin"
};
static const char module_4[] = {
	#embed lzo "ovl4.bin"
};
static const char module_5[] = {
	#embed lzo "ovl5.bin"
};
static const char module_6[] = {
	#embed lzo "ovl6.bin"
};

static const char * const modules[OVERLAYS] = {
	module_1, module_2, module_3, module_4, module_5, module_6
};

int main(void)
{
	for (byte i = 0; i < OVERLAYS; ++i)
	{
		oscar_expand_lzo((char *)overlay_code, modules[i]);
		print_overlay();
	}
	return 0;
}
