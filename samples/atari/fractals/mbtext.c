#include "color_cells.h"

int main(void)
{
	ColorCellsState saved;
	show_cells(&saved);

	for (byte py = 0; py < ROWS; ++py)
	{
		for (byte px = 0; px < COLUMNS; ++px)
		{
			float xz = px * (3.5 / COLUMNS) - 2.5;
			float yz = py * (2.0 / ROWS) - 1.0;
			float x = 0.0, y = 0.0;
			byte i;
			for (i = 0; i <= 14; ++i)
			{
				float xx = x * x, yy = y * y;
				if (xx + yy > 4.0)
					break;
				y = 2 * x * y + yz;
				x = xx - yy + xz;
			}
			cells[py][px] = pixels[i - 1];
		}
	}

	key_get();
	close_cells(&saved);
	return 0;
}
