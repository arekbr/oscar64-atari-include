#ifndef ATARI_PMG_H
#define ATARI_PMG_H

#include "types.h"
#include "gtia.h"

// ANTIC DMA supplies four 8-bit players and four 2-bit missiles to GTIA.
// Provide a 2048-byte buffer aligned to 2048 bytes for PMG_1LINE,
// or a 1024-byte buffer aligned to 1024 bytes for PMG_2LINE.
// Each byte covers one scan line in PMG_1LINE or two in PMG_2LINE.
// Write through the volatile pointers so stores read only by DMA are retained.
//
// Set horizontal positions in color clocks with gtia.hposp/gtia.hposm.
// The caller sets sizes, colors, priority and vertical delay.
// With the OS vertical blank handler, set colors through os_pcolr and
// priority through os_gprior. Read player collisions with the playfield from
// gtiar.ppf[n] (bit k for PFk) and with other players from gtiar.ppl[n].
// Write gtia.hitclr to clear the collision latches.

#define PMG_2LINE	0
#define PMG_1LINE	1

// Clear the player and missile planes, set PMBASE, and enable GTIA graphics.
// ANTIC DMA and resolution take effect when the OS copies os_sdmctl to DMACTL.
void pmg_init(volatile byte * base, byte resolution);

// Disable PMG, zero hardware positions and graphics registers, and clear
// the PMG bits in os_sdmctl. The graphics buffer remains intact.
void pmg_off(void);

// After pmg_init, return player n (0 through 3). In one-line mode, its plane
// starts at base + $400 + n * $100 (256 bytes); in two-line mode, it starts
// at base + $200 + n * $80 (128 bytes).
volatile byte * pmg_player(byte n);
// Number of rows per plane after pmg_init(), or zero before initialization.
word pmg_plane_rows(void);

// After pmg_init, return the shared missile plane at base + $300 in one-line
// mode or base + $180 in two-line mode. Bits 2*n and 2*n+1 belong to missile n.
volatile byte * pmg_missiles(void);

// Clear all active DMA planes and tracked shape spans. Positions stay set.
void pmg_clear(void);

// Compose one missile size field. A size of 2 maps to normal as on GTIA.
byte pmg_missile_size(byte missile, byte size);

struct PMGShape
{
	const byte * scanlines;
	word count;
	byte x, y;
};

// Replace the tracked shape and position, removing all previous scanlines
// for that object. Missile values use the low two bits. The other missiles
// are preserved. A zero count hides the object. Shapes clip at the bottom.
// Keep source bytes outside the PMG DMA buffers. Use after pmg_init.
bool pmg_move_player(byte player, const PMGShape * shape);
bool pmg_move_missile(byte missile, const PMGShape * shape);

#pragma compile("pmg.c")

#endif
