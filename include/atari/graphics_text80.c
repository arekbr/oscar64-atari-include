#include "graphics_text80.h"
#include "charwin.h"

static bool text_layout(const ABitmap * bitmap)
{
	return bitmap && bitmap->data && bitmap->width == 4 * GR_TEXT80_COLS &&
		bitmap->height == 8 * GR_TEXT80_ROWS && bitmap->bits == 1 &&
		bitmap->pitch >= GR_TEXT80_COLS / 2;
}

static void draw_cell(const ABitmap * bitmap, byte column, byte row, byte ch, const byte * font)
{
	byte code = cwin_a2s(ch);
	const byte * glyph = font + ((word)(code & 0x7f) << 3);
	volatile byte * pixels = abm_row(bitmap, row << 3) + (column >> 1);
	byte inverse = code & 0x80 ? 15 : 0;
	byte shift = column & 1 ? 0 : 4;
	byte keep = (byte)~(15 << shift);
	for (byte y = 0; y < 8; y++)
	{
		byte ink = ((glyph[y] & 15) ^ inverse) << shift;
		volatile byte * pair = pixels + (word)y * bitmap->pitch;
		*pair = (*pair & keep) | ink;
	}
}

bool gr_text80_put(const ABitmap * bitmap, byte column, byte row, byte ch, const byte * font)
{
	if (!font || column >= GR_TEXT80_COLS || row >= GR_TEXT80_ROWS || !text_layout(bitmap))
		return false;
	draw_cell(bitmap, column, row, ch, font);
	return true;
}

byte gr_text80_write(const ABitmap * bitmap, byte column, byte row, const char * text, const byte * font)
{
	if (!text || !font || row >= GR_TEXT80_ROWS || !text_layout(bitmap))
		return 0;
	byte first = column;
	while (column < GR_TEXT80_COLS && *text)
		draw_cell(bitmap, column++, row, *text++, font);
	return column - first;
}

bool gr_text80_scroll(const ABitmap * bitmap, byte rows)
{
	if (!text_layout(bitmap))
		return false;
	if (rows >= GR_TEXT80_ROWS)
		abm_clear(bitmap, 0);
	else if (rows)
	{
		byte top = rows << 3;
		ABitmapRect source = {0, top, bitmap->width, bitmap->height - top};
		if (!abm_copy_rect(bitmap, 0, 0, bitmap, &source))
			return false;
		abm_fill_rect(bitmap, 0, source.height, bitmap->width, top, 0);
	}
	return true;
}
