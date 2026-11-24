#include "../support/stars.h"

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODEE, 200))
		return 1;
	stars(true);
	key_get();
	bitmap_demo_restore(&saved);
	return 0;
}
