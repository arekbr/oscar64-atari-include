#include "../support/surface.h"

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEF, 200))
		return 1;
	surface(false);
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
