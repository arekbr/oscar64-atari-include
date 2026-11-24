#ifndef ATARI_CHAR_SPRITE_ANIMATION_H
#define ATARI_CHAR_SPRITE_ANIMATION_H

#include "char_sprite.h"

#define CHAR_SPRITE_LOOP 1
#define CHAR_SPRITE_HIDE_AT_END 2

struct ACharSpriteFrame
{
	const byte * pixels;
	word size;
	byte ticks;
};

struct ACharSpriteClip
{
	const ACharSpriteFrame * frames;
	byte count, columns, rows, pitch, flags;
};

struct ACharSpriteTrack
{
	const ACharSpriteClip * clip;
	ACharSprite sprite;
	int velocity_x, velocity_y;
	byte frame, elapsed;
	bool finished;
};

// Start a clip at frame zero with origin and velocity zero. Clip and frame
// data remain immutable while in use. A zero-initialized track is empty.
bool char_sprite_play(ACharSpriteTrack * track, const ACharSpriteClip * clip);
// Advance the clock and move by velocity times ticks. Coordinates
// saturate to the signed 16-bit limits. A finished one-shot holds
// its last frame unless the clip sets CHAR_SPRITE_HIDE_AT_END.
void char_sprite_step(ACharSpriteTrack * track, byte ticks);

// Translate the tracks into caller-owned descriptors and composite them
// in order. Provide space for count sprites, separate from tracks and
// renderer storage. The caller publishes the finished off-screen frame.
byte char_sprite_tracks(const ACharSpriteScreen * screen, const ACharSpriteTrack * tracks,
	byte count, ACharSprite * work, byte capacity);

#pragma compile("char_sprite_animation.c")

#endif
