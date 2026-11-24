#ifndef ATARI_CHAR_SPRITE_VIDEO_H
#define ATARI_CHAR_SPRITE_VIDEO_H

#include "char_sprite.h"

struct ACharSpriteVideo
{
	const ACharSpriteScreen * screen;
	volatile byte * list;
	word list_size;
	byte mode, top;
};

// Present a prepared ANTIC 4/5 sprite screen using 32, 40 or 48 columns.
// The display list switches a 1 KiB font at each band boundary. The
// frame fits within 224 scanlines including the top margin.
// Call from the main program. While active, owns DLI and chains to
// the previous immediate VBI. Keep the list, screen and fonts in RAM.
bool char_sprite_video_show(const ACharSpriteVideo * video);
// Restore display shadows and the saved immediate VBI. Leave DLI off.
void char_sprite_video_off(void);


#define CHAR_SPRITE_PMG_ROWS 28

struct ACharSpritePmgActor
{
	const ACharSprite * sprite;
	byte first, second;
};

struct ACharSpritePmgFrame
{
	volatile byte * players;
	byte columns, rows, mode, top;
	byte position[2][CHAR_SPRITE_PMG_ROWS], color[4][CHAR_SPRITE_PMG_ROWS];
};

// Build a frame in separate RAM with a disjoint 2 KiB-aligned players arena.
// Actors use the same geometry and packed pixels as character sprites, up to
// eight pixels wide. Null sprite pointers hide actors. Pixel values 1 and 2
// use the two specified GTIA colors, 3 combines them, and 0 is transparent.
// A pair of players can host one actor per text row. Reusing it for a new
// actor requires a free preceding text row for the DLI to set up the pair.
// CHAR_SPRITE_FULL means an actor did not fit and was skipped. Build into
// an unpublished arena. The underlying character screen still works.
byte char_sprite_pmg_render(ACharSpritePmgFrame * frame, const ACharSpriteVideo * video,
	const ACharSpritePmgActor * actors, byte count);

// Give actor 0 first choice of a player pair. Visit the other actors in
// descending cyclic order. Keep *next in separate caller-owned storage,
// initially zero. The first actor skipped for lack of a pair is tried
// first among these actors on the next call. If all fit, keep the starting
// actor. A rejected frame leaves *next unchanged.
byte char_sprite_pmg_render_rotating(ACharSpritePmgFrame * frame, const ACharSpriteVideo * video,
	const ACharSpritePmgActor * actors, byte count, byte * next);

// Present with independent two-color PMG actors composed over the screen.
// PMG DMA must be free. While active the library owns the player registers.
// Keep the displayed arena immutable until update or off returns.
bool char_sprite_video_show_pmg(const ACharSpriteVideo * video, const ACharSpritePmgFrame * frame);
// Swap to a disjoint arena rendered for this video during the next VBI.
bool char_sprite_video_update_pmg(const ACharSpritePmgFrame * frame);

#pragma compile("char_sprite_video.c")

#endif
