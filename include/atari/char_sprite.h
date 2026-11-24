#ifndef ATARI_CHAR_SPRITE_H
#define ATARI_CHAR_SPRITE_H

#include "types.h"

#define CHAR_SPRITE_BAD 0
#define CHAR_SPRITE_OK 1
#define CHAR_SPRITE_FULL 2

struct ACharSprite
{
	const byte * pixels;
	word size;
	int x, y;
	byte columns, rows, pitch;
};

struct ACharSpriteScreen
{
	const byte * background, * font;
	volatile byte * output, * fonts;
	word cells, font_size;
	byte columns, rows, rows_per_font, first_slot;
};

// Prepare the fixed glyphs in each output charset page. Use ANTIC 4 or 5
// and select each 1 KiB page through CHBASE for its band of text rows.
// Pages must be aligned on a 1 KiB boundary.
bool char_sprite_prepare(const ACharSpriteScreen * screen);

// Rebuild the output from the background and draw the sprites in order.
// Sprites contain four packed 2-bit pixels per byte, with color zero clear.
// Coordinates use bitmap pixels and font rows. In ANTIC 5 one font row
// is displayed twice. A nonzero background inverse bit is retained.
// Screen codes must use fixed slots below first_slot. Source storage
// must be separate from the output and fonts. Use an off-screen buffer
// when the published screen must remain stable throughout rendering.
// CHAR_SPRITE_FULL leaves the available glyphs drawn and skips cells
// requiring additional glyph storage.
byte char_sprite_render(const ACharSpriteScreen * screen, const ACharSprite * sprites, byte count);

#pragma compile("char_sprite.c")

#endif
