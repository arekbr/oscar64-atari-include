#include "char_sprite.h"

struct Span
{
	word start, end;
};

static bool range(Span * span, word start, word size)
{
	if (!start || !size || start > 0xffffu - (size - 1))
		return false;
	span->start = start;
	span->end = start + (size - 1);
	return true;
}

static bool overlap(const Span * a, const Span * b)
{
	return a->start <= b->end && b->start <= a->end;
}

struct ScreenLayout
{
	Span output, fonts;
	word cells;
	byte bands;
};

static bool screen_layout(ScreenLayout * layout, const ACharSpriteScreen * screen)
{
	if (!screen || !screen->columns || !screen->rows || !screen->rows_per_font ||
		!screen->first_slot || screen->first_slot >= 128)
		return false;
	word count = (word)screen->columns * screen->rows;
	word bands = ((word)screen->rows + screen->rows_per_font - 1) / screen->rows_per_font;
	if (bands > 32 || screen->cells < count || screen->font_size < (bands << 10) || ((word)screen->fonts & 1023u))
		return false;
	Span fixed, background, descriptor;
	if (!range(&fixed, (word)screen->font, (word)screen->first_slot * 8) ||
		!range(&background, (word)screen->background, count) ||
		!range(&descriptor, (word)screen, sizeof(ACharSpriteScreen)) ||
		!range(&layout->output, (word)screen->output, count) ||
		!range(&layout->fonts, (word)screen->fonts, bands << 10) ||
		overlap(&layout->output, &layout->fonts) ||
		overlap(&layout->fonts, &fixed) || overlap(&layout->fonts, &background) ||
		overlap(&layout->output, &fixed) || overlap(&layout->output, &background) ||
		overlap(&layout->fonts, &descriptor) || overlap(&layout->output, &descriptor))
		return false;
	layout->bands = bands;
	layout->cells = count;
	return true;
}

bool char_sprite_prepare(const ACharSpriteScreen * screen)
{
	ScreenLayout layout;
	if (!screen_layout(&layout, screen))
		return false;
	word count = (word)screen->first_slot * 8;
	for (byte band = 0; band < layout.bands; band++)
	{
		volatile byte * page = screen->fonts + ((word)band << 10);
		for (word i = 0; i < count; i++)
			page[i] = screen->font[i];
	}
	return true;
}

static bool sprite_ok(const ScreenLayout * layout, const ACharSprite * sprite)
{
	if (!sprite->columns || !sprite->rows)
		return true;
	if (sprite->pitch < sprite->columns)
		return false;
	word extent = (word)(sprite->rows - 1) * sprite->pitch + sprite->columns;
	Span pixels;
	return extent <= sprite->size && range(&pixels, (word)sprite->pixels, extent) &&
		!overlap(&layout->output, &pixels) && !overlap(&layout->fonts, &pixels);
}

struct Run
{
	const ACharSpriteScreen * screen;
	byte free[32];
	bool full;
};

static byte sprite_byte(const ACharSprite * sprite, const byte * row, int column)
{
	int left = column >> 2;
	byte shift = ((byte)column & 3) << 1;
	byte result = left < 0 ? 0 : row[left] << shift;
	if (shift && left + 1 < sprite->columns)
		result |= row[left + 1] >> (8 - shift);
	return result;
}

struct SpriteCell
{
	byte x, y, first_line, last_line;
};

static void draw_cell(Run * run, const ACharSprite * sprite, const SpriteCell * cell)
{
	const ACharSpriteScreen * screen = run->screen;
	byte ink[8], total = 0;
	int sx = (word)cell->x * 4 - sprite->x;
	word sy = (word)cell->y * 8 + cell->first_line - sprite->y;
	const byte * row = sprite->pixels + sy * sprite->pitch;
	for (byte y = cell->first_line; y < cell->last_line; y++, row += sprite->pitch)
	{
		ink[y] = sprite_byte(sprite, row, sx);
		total |= ink[y];
	}
	if (!total)
		return;
	word pos = (word)cell->y * screen->columns + cell->x;
	volatile byte * code = screen->output + pos;
	byte old = *code, slot = old & 0x7f;
	byte band = cell->y / screen->rows_per_font;
	volatile byte * page = screen->fonts + ((word)band << 10);
	if (slot < screen->first_slot)
	{
		byte next = run->free[band];
		if (next >= 128)
		{
			run->full = true;
			return;
		}
		run->free[band] = next + 1;
		const byte * background = screen->font + (word)slot * 8;
		volatile byte * glyph = page + (word)next * 8;
		for (byte y = 0; y < 8; y++)
			glyph[y] = background[y];
		*code = (old & 0x80) | next;
		slot = next;
	}
	volatile byte * glyph = page + (word)slot * 8;
	for (byte y = cell->first_line; y < cell->last_line; y++)
	{
		byte pixels = ink[y];
		byte mask = (pixels | (pixels >> 1)) & 0x55;
		mask |= mask << 1;
		glyph[y] = (glyph[y] & ~mask) | pixels;
	}
}

static void draw_sprite(Run * run, const ACharSprite * sprite)
{
	if (!sprite->columns || !sprite->rows)
		return;
	int left = sprite->x, top = sprite->y;
	long right = (long)left + (word)sprite->columns * 4;
	long bottom = (long)top + sprite->rows;
	word limit_x = (word)run->screen->columns * 4, limit_y = (word)run->screen->rows * 8;
	if (right > limit_x)
		right = limit_x;
	if (bottom > limit_y)
		bottom = limit_y;
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;
	if (right <= left || bottom <= top)
		return;
	byte x0 = left >> 2, x1 = (right - 1) >> 2;
	byte y0 = top >> 3, y1 = (bottom - 1) >> 3;
	SpriteCell cell;
	for (byte y = y0; y <= y1; y++)
	{
		cell.y = y;
		cell.first_line = y == y0 ? top & 7 : 0;
		cell.last_line = y == y1 ? ((bottom - 1) & 7) + 1 : 8;
		for (byte x = x0; x <= x1; x++)
		{
			cell.x = x;
			draw_cell(run, sprite, &cell);
		}
	}
}

byte char_sprite_render(const ACharSpriteScreen * screen, const ACharSprite * sprites, byte count)
{
	ScreenLayout layout;
	if (!screen_layout(&layout, screen))
		return CHAR_SPRITE_BAD;
	if (count)
	{
		Span descriptors;
		if (!range(&descriptors, (word)sprites, (word)count * sizeof(ACharSprite)) ||
			overlap(&layout.output, &descriptors) || overlap(&layout.fonts, &descriptors))
			return CHAR_SPRITE_BAD;
		for (byte i = 0; i < count; i++)
			if (!sprite_ok(&layout, sprites + i))
				return CHAR_SPRITE_BAD;
	}
	for (word i = 0; i < layout.cells; i++)
		if ((screen->background[i] & 0x7f) >= screen->first_slot)
			return CHAR_SPRITE_BAD;
	for (word i = 0; i < layout.cells; i++)
		screen->output[i] = screen->background[i];
	Run run;
	run.screen = screen;
	run.full = false;
	for (byte band = 0; band < layout.bands; band++)
		run.free[band] = screen->first_slot;
	for (byte i = 0; i < count; i++)
		draw_sprite(&run, sprites + i);
	return run.full ? CHAR_SPRITE_FULL : CHAR_SPRITE_OK;
}
