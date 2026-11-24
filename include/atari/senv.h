#ifndef ATARI_SENV_H
#define ATARI_SENV_H

#include "types.h"
#include "sound.h"

// Software ADSR envelopes for the four POKEY audio channels. The installed
// handler advances them once per frame in the immediate vertical blank
// interrupt, including during critical OS sections that skip deferred VBI.
//
// Attack rises from zero to the peak. Decay falls to the sustain level and
// holds there until note-off. Release falls from the current level to zero.
// Attack, decay, and release durations count video frames. A zero duration
// jumps to the target. The tick accepting a command takes the first step.
// After a nonzero segment ends, the next segment steps on the following tick.
//
// Levels use 8.8 fixed point. The peak is peak * 256 and the sustain target
// is floor(peak * 256 * sustain / 15). After k of N steps, a segment has
// moved floor(k * abs(target - start) / N) toward its target. POKEY volume
// is the integer part of the current level.
//
// Call init, shutdown, note-on and note-off from the main program. Each
// channel has one pending command. If occupied, the next command waits for
// a tick to consume it. Keep ticks running while submitting commands. When
// driving senv_tick yourself, advance it between commands to the same channel.
//
// The module writes AUDF and AUDC for the channels it drives. Keep them
// separate from any POKEY timers and other audio code. Direct sound calls
// can use idle channels with no pending note. The module leaves AUDCTL
// to the caller. OS serial I/O changes AUDCTL and channels 3 and 4, so
// envelope timing continues but the resulting sound can change.

#define SENV_IDLE		0
#define SENV_ATTACK		1
#define SENV_DECAY		2
#define SENV_SUSTAIN	3
#define SENV_RELEASE	4

struct SEnv
{
	byte	attack, decay;		// Duration in frames. Zero jumps to the target.
	byte	sustain;			// Fraction of peak, 0..15.
	byte	release;			// Frames to silence. Zero mutes on the note-off tick.
};

// AUDF for a pure tone using PAL's 64 kHz clock (AUDCTL = 0).
// hz must be positive and yield an AUDF value in 0..255.
#define SOUND_AUDF(hz)	((byte)((633374UL + 10UL * (hz)) / (20UL * (hz)) - 1))

// PAL pure-tone AUDF values for C3 through B5, in semitone order.
// Index 0 is C3, 12 is C4, and 21 is A4.
extern const byte snd_note[36];

// Install and chain an immediate VBI handler. Repeated calls have no effect.
// When sharing VVBLKI with other handlers, remove them in reverse order.
void senv_init(void);

// Restore the previous immediate VBI and mute the active envelope channels.
// Discard pending commands. Calling shutdown without an active install
// has no effect.
void senv_shutdown(void);

// Queue a note for channel ch & 3. audf is the POKEY divider, dist is even
// from 0 to 14, and peak is 0..15. A new note starts from zero in any state.
// The descriptor is copied before the function returns.
void senv_on(byte ch, byte audf, byte dist, byte peak, const struct SEnv * e);

// Queue release from the level at the accepting tick. A command in IDLE or
// RELEASE leaves the envelope running normally.
void senv_off(byte ch);

// Read the envelope's state and volume. These describe the software envelope
// and cannot report direct writes to POKEY.
byte senv_state(byte ch);
byte senv_vol(byte ch);

// Advance all channels once. Call from your own handler or a manual
// tick loop only when the module's immediate VBI is inactive.
__interrupt void senv_tick(void);

// Diagnostic count of tick-driven AUDC writes per channel, modulo 256.
extern volatile byte senv_writes[4];

#pragma compile("senv.c")

#endif
