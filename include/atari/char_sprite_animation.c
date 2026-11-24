#include "char_sprite_animation.h"

static bool range(word address, word count)
{
	return address && count && address <= 0xffffu - (count - 1);
}

static void select_frame(ACharSpriteTrack * track, byte index)
{
	track->frame = index;
	const ACharSpriteFrame * frame = track->clip->frames + index;
	track->sprite.pixels = frame->pixels;
	track->sprite.size = frame->size;
}

bool char_sprite_play(ACharSpriteTrack * track, const ACharSpriteClip * clip)
{
	if (!range((word)track, sizeof(ACharSpriteTrack)) || !range((word)clip, sizeof(ACharSpriteClip)) ||
		!clip->columns || !clip->rows || clip->pitch < clip->columns ||
		clip->flags > CHAR_SPRITE_HIDE_AT_END ||
		!range((word)clip->frames, (word)clip->count * sizeof(ACharSpriteFrame)))
		return false;
	word used = (word)(clip->rows - 1) * clip->pitch + clip->columns;
	for (byte i = 0; i < clip->count; i++)
	{
		const ACharSpriteFrame * frame = clip->frames + i;
		if (!frame->ticks || frame->size < used || !range((word)frame->pixels, used))
			return false;
	}
	track->clip = clip;
	track->sprite.x = track->sprite.y = 0;
	track->sprite.columns = clip->columns;
	track->sprite.rows = clip->rows;
	track->sprite.pitch = clip->pitch;
	track->velocity_x = track->velocity_y = 0;
	track->finished = false;
	track->elapsed = 0;
	select_frame(track, 0);
	return true;
}

static int move(int position, int speed, byte ticks)
{
	long next = (long)position + (long)speed * ticks;
	if (next > 32767l)
		return 32767;
	if (next < -32768l)
		return -32768l;
	return (int)next;
}

void char_sprite_step(ACharSpriteTrack * track, byte ticks)
{
	if (!track || !track->clip || !ticks)
		return;
	track->sprite.x = move(track->sprite.x, track->velocity_x, ticks);
	track->sprite.y = move(track->sprite.y, track->velocity_y, ticks);
	if (track->finished)
		return;
	const ACharSpriteClip * clip = track->clip;
	word elapsed = (word)track->elapsed + ticks;
	byte index = track->frame;
	byte duration = clip->frames[index].ticks;
	while (elapsed >= duration)
	{
		elapsed -= duration;
		if (index + 1 < clip->count)
			index++;
		else if (clip->flags & CHAR_SPRITE_LOOP)
			index = 0;
		else
		{
			track->finished = true;
			elapsed = 0;
			break;
		}
		duration = clip->frames[index].ticks;
	}
	track->elapsed = elapsed;
	if (index != track->frame)
		select_frame(track, index);
}

byte char_sprite_tracks(const ACharSpriteScreen * screen, const ACharSpriteTrack * tracks,
	byte count, ACharSprite * work, byte capacity)
{
	if (count)
	{
		word source = (word)tracks, dest = (word)work;
		word n = (word)count * sizeof(ACharSpriteTrack), m = (word)count * sizeof(ACharSprite);
		if (capacity < count || !range(source, n) || !range(dest, m) ||
			(source <= dest + (m - 1) && dest <= source + (n - 1)))
			return CHAR_SPRITE_BAD;
	}
	for (byte i = 0; i < count; i++)
	{
		const ACharSpriteTrack * track = tracks + i;
		if (!track->clip || (track->finished && (track->clip->flags & CHAR_SPRITE_HIDE_AT_END)))
		{
			work[i].columns = 0;
			work[i].rows = 0;
		}
		else
			work[i] = track->sprite;
	}
	return char_sprite_render(screen, work, count);
}
