#ifndef ATARI_PSEQ_H
#define ATARI_PSEQ_H
#include "types.h"
// Use the same PSQ_EXTENDED setting throughout the program. With 0
// (default), instruments and banks are 6 and 9 bytes. With 1, they are
// 7 and 12 bytes and support arpeggios, masked effects, and voice overrides.
#ifndef PSQ_EXTENDED
#define PSQ_EXTENDED 0
#endif
#if PSQ_EXTENDED != 0 && PSQ_EXTENDED != 1
#error PSQ_EXTENDED must be 0 or 1 for the entire compilation
#endif

// With PSQ_SEPARATE_BSS=1, place psq_state in a caller-owned region and clear
// that section before any PSQ call or interrupt can run. Only the module's
// mutable state goes there. Banks and their tables stay in caller memory.
#ifndef PSQ_SEPARATE_BSS
#define PSQ_SEPARATE_BSS 0
#endif
#if PSQ_SEPARATE_BSS != 0 && PSQ_SEPARATE_BSS != 1
#error "PSQ_SEPARATE_BSS must be 0 or 1 for the entire compilation"
#endif
#if PSQ_SEPARATE_BSS
#pragma section(psq_state,0, , ,bss)
#endif

// Keep the asynchronous call tree on the dynamic stack so its locals
// cannot overlap those of interrupted code. PSQ_IRQ_STATIC is for tests.
#ifdef PSQ_IRQ_STATIC
#define PSQ_ASYNC
#else
#define PSQ_ASYNC __dynstack
#endif

// Track 0 uses joined POKEY channels 1 and 2 for a 16-bit pure tone.
// Tracks 1 and 2 use channels 3 and 4 with the 15 kHz clock. AUDCTL is $51.
// Give the player exclusive control of AUDF, AUDC and AUDCTL on every
// unsuspended tick, including after STOP. Suspend and wait for acknowledgement
// before OS serial I/O uses POKEY.
#define PSQ_OK          0
#define PSQ_BUSY        1
#define PSQ_BAD_BANK    2
#define PSQ_BAD_SONG    3
#define PSQ_BAD_TEMPO   4
#define PSQ_NOT_INIT    5
#define PSQ_BAD_MASK    6
#define PSQ_BAD_CONTROL 7
#define PSQ_NO_LOOP     0xffffu
#define PSQ_MAX_EVENTS  16383u

// Notes are one-based, with 0 for a rest. Instruments are zero-based.
// Duration is 1..255 units. Gate 0 holds through the event. A nonzero gate
// is at most duration and starts release after that many units.
struct PSQEvent { byte note, instrument, duration, gate; };
struct PSQNote { word lead; byte bass, arp; };
// Peak and sustain are absolute volumes in 0..15, with sustain <= peak.
// Attack, decay and release are volume steps per frame (0 jumps to the
// target). Distortion is the AUDC field in bits 5..7.
#if PSQ_EXTENDED
// Four note-index offsets with a nonzero period in frames. Instrument arp
// is one-based, with 0 for no arpeggio.
struct PSQArp { sbyte offset[4]; byte period; };
struct PSQInstrument { byte peak, sustain, attack, decay, release, distortion, arp; };
#else
struct PSQInstrument { byte peak, sustain, attack, decay, release, distortion; };
#endif
// A track has at most PSQ_MAX_EVENTS events. loop is a zero-based event
// index or PSQ_NO_LOOP. For an empty track use count 0 and PSQ_NO_LOOP.
struct PSQTrack { const struct PSQEvent * events; word count, loop; };
struct PSQSong { struct PSQTrack track[3]; };
struct PSQBank {
	const struct PSQNote * notes;       // note1 = notes[0]; 0 = rest
	const struct PSQInstrument * instruments;
	const struct PSQSong * songs;
	byte note_count, instrument_count, song_count;
#if PSQ_EXTENDED
	const struct PSQArp * arps;
	byte arp_count;
#endif
};
#if PSQ_EXTENDED
// Raw dividers and AUDC values for the fixed POKEY layout.
// Voices 1 and 2 require an enabled divider in 0..255.
struct PSQOverride { word divider; byte audc, enabled; };
struct PSQControls { byte mask, tempo; struct PSQOverride voice[3]; };
#endif

// Validate and install a whole bank from main code while ticks are stopped.
// A failed init keeps the old bank. Init does not write hardware registers.
// Keep the bank and all referenced arrays immutable and readable at every tick.
__noinline byte psq_init(const struct PSQBank * bank);
// One main producer posts transport commands. PSQ_OK means queued, and
// PSQ_BUSY means the previous command is still pending. BUSY leaves that
// command intact. The next tick consumes the published parameters.
__noinline byte psq_play(byte song);
__noinline byte psq_stop(void);
__noinline byte psq_tempo(byte frames_per_unit);  // 1..255 frames per base unit, starting at the next event
// Play an effect on track 2 and pause its base track until the effect ends.
// Lower priority is ignored by the tick. Equal or higher priority replaces
// the effect. Priority 0 is valid. FX duration and gate count PAL frames.
// The base track resumes with the tempo saved for its current event.
__noinline byte psq_fx(byte song, byte priority);
#if PSQ_EXTENDED
// Select tracks with mask bits 0..2 (mask 1..7). Their bases pause while
// unselected tracks continue. The effect ends when every selected track and
// its release finish. Replacement effects keep the saved base state.
__noinline byte psq_fx_mask(byte song, byte priority, byte mask);
// Restart the selected base tracks, including empty tracks. Active effects,
// overrides, tempo, suspend state and unselected tracks stay in place.
__noinline byte psq_base(byte song, byte mask);
// Count effects accepted by the tick, modulo 256, including same-priority
// replacements. Only successful init resets the count. Use one pending FX
// when checking acceptance. Effect completion is a separate event.
__noinline byte psq_fx_generation(void);
// Queue controls in a separate nonblocking mailbox. mask selects which
// overrides to update. tempo 0 preserves the current tempo. The tick applies
// controls before transport, so PLAY and STOP clear all overrides and a
// transport tempo wins. Effects take precedence over overrides and bases.
// Disabling an override exposes the base when no effect owns the voice.
__noinline byte psq_controls(const struct PSQControls * controls);
__noinline byte psq_controls_pending(void);
#endif
__noinline byte psq_suspend(void);               // Tick mutes, freezes playback and pauses writes.
__noinline byte psq_resume(void);                // Tick restores the sequencer's audio registers.
// Tick once per PAL frame from one consumer. Use either the main loop or
// a VBI wrapper that preserves interrupted compiler state and uses a dynamic
// stack callback. The player installs no interrupts. Tick performs no waits,
// allocations or OS calls.
PSQ_ASYNC __noinline void psq_tick(void);
__noinline byte psq_playing(void);               // Includes a queued PLAY and an active event or release.
__noinline byte psq_suspended(void);             // Wait for acknowledgement before CIO/SIO.
__noinline byte psq_pending(void);
// Diagnostic image of normal tick writes, from AUDF1 through AUDCTL.
// The suspend tick mutes AUDC directly and preserves this image.
extern volatile byte psq_regs[9];
#pragma compile("pseq.c")
#endif
