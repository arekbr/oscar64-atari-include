#include <math.h>
#include "bitmap_sprites.h"

static sbyte sine[128];

int main(void)
{
	SpriteVideoState saved;
	if (!start_sprites(&saved))
		return 1;
	for (byte i = 0; i < 128; ++i)
		sine[i] = (sbyte)(70 * sin(i * PI / 64));

	word j = 0;
	while (!(cons_keys() & CONSOL_START))
	{
		start_frame();
		byte k = j >> 4;
		for (byte i = 16; i > 0; --i)
		{
			byte n = i - 1;
			byte angle = (j + 8 * n) & 127;
			int x = 200 + sine[angle] + sine[k & 127];
			int y = 150 + sine[(angle + 32) & 127] + sine[(k + 32) & 127];
			draw_sprite(x, y, n);
		}
		abm_screen_present(&display);
		++j;
	}
	stop_sprites(&saved);
	return 0;
}
