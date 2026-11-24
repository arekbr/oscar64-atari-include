#ifndef ATARI_VPMG_H
#define ATARI_VPMG_H

#include "types.h"

// Display up to VPMG_MAX virtual players using GTIA's four hardware players.
// A player's image specifies its vertical extent in single-line PMG memory.
// A display list interrupt (DLI) changes horizontal positions and colors
// when a new vertical zone starts.
//
// A boundary B is the first scan line following the display list instruction
// with DL_DLI set. List boundaries in the same order as the frame's DLIs.
// A player may cross zone boundaries and keep its position and color. A
// hardware player can switch to another image at B when the previous image
// ends by B - 2 and the next starts at or after B + VPMG_GUARD. The gap
// gives the DLI time to change registers while this hardware player is blank.
//
// vpmg_update builds the next frame in a back buffer, then requests a swap.
// The immediate vertical blank interrupt (VBI) switches PMBASE and the DLI
// tables together. It also sets the first zone's colors and positions each
// frame and updates the OS player-color shadows.
//
// Use the API from the main program. While active, it owns VDSLST and the
// immediate VBI slot. It chains to the previous immediate VBI. Give it sole
// control of the DLI vector and player positions, colors, and DMA settings.

#ifndef VPMG_MAX
#define VPMG_MAX	12
#endif
#define VPMG_ZONES	8		// Maximum boundaries. Zone count is one greater.
#ifndef VPMG_GUARD
#define VPMG_GUARD	2		// Reserved scan lines starting at each boundary.
#endif
#define VPMG_LAST	232		// Latest boundary, before vertical blank.
#define VPMG_HMAX	32		// Maximum image height in scan lines.
#define VPMG_BUSY	255

// Supply two separate 2 KiB buffers, each aligned to 2 KiB. Keep both in
// writable RAM while the multiplexer uses them. Init clears them and selects
// single-line DMA, normal-width players, no vertical delay, and no missiles.
// Call before starting the multiplexer or after stopping it. All virtual
// players start hidden in a single zone.
void vpmg_init(volatile byte * pm0, volatile byte * pm1);

// Set boundaries while stopped. Supply at most VPMG_ZONES values in ascending
// order, at least VPMG_GUARD + 2 lines apart and no later than VPMG_LAST.
// Pass n = 0 for a single zone. The function copies the boundary information.
// It rejects an invalid request with false and preserves the existing zones.
// Extra DLIs after the last boundary have no effect. Changing zones discards
// both frame plans. Call vpmg_update to prepare a frame for the new zones.
bool vpmg_zones(const byte * lines, byte n);

// sp is a virtual player index in 0..VPMG_MAX-1. x is the GTIA HPOS value in
// color clocks. y is the scan line. Supply h bytes for img with h in 1..VPMG_HMAX
// and y + h <= 256. Invalid height or vertical placement hides the player.
// Keep img valid for each update that uses it. The update copies its bytes
// into PMG memory, so interrupt handlers use the prepared frame. color is
// the GTIA color byte for this player.
void vpmg_set(byte sp, byte x, byte y, const byte * img, byte h, byte color);
void vpmg_move(byte sp, byte x, byte y);		// Make visible if its placement is valid.
void vpmg_hide(byte sp);

// Return the number of visible virtual players that could not be placed.
// VPMG_BUSY means the previous frame is awaiting its VBI swap. In that case
// the update leaves both buffers and the pending frame alone.
byte vpmg_update(void);

// Count frame plans prepared by the caller and swaps completed by the VBI.
word vpmg_prepared(void);
word vpmg_shown(void);

// Install the DLI and immediate VBI, then enable DLIs at vertical blank.
void vpmg_start(void);

// Disable DLIs and restore the previous immediate VBI. The multiplexer
// leaves its DLI vector and PMG configuration in place.
void vpmg_stop(void);

#pragma compile("vpmg.c")

#endif
