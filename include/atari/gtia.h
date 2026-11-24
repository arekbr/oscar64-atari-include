#ifndef ATARI_GTIA_H
#define ATARI_GTIA_H

#include "types.h"

// GTIA ($D000-$D01F). Reads and writes address different registers.
// Use gtia for writes and gtiar for reads.
//
// Colors use the high nibble for hue and an even value (0-14) for luminance.
// With the OS vertical blank handler active, it copies the color shadows
// to the hardware registers each frame.

#define ATARI_COLOR(hue, lum)	((byte)(((hue) << 4) | ((lum) & 0x0e)))

// PRIOR
#define PRIOR_P0123_PF0123	0x01	// players in front of playfield
#define PRIOR_P01_PF_P23	0x02
#define PRIOR_PF0123_P0123	0x04	// playfield in front of players
#define PRIOR_PF01_P_PF23	0x08
#define PRIOR_FIFTH_PLAYER	0x10	// balls in COLPF3 colour
#define PRIOR_MULTICOLOR	0x20	// Combine overlapping players 0/1 and 2/3 by ORing their colors.
#define PRIOR_GTIA_MODE9	0x40	// 16 brightness levels of one hue
#define PRIOR_GTIA_MODE10	0x80	// 9 colours from registers
#define PRIOR_GTIA_MODE11	0xc0	// 16 hues of one brightness

// GRACTL
#define GRACTL_MISSILES		0x01	// GTIA fetches ball data from DMA
#define GRACTL_PLAYERS		0x02	// GTIA fetches player data from DMA
#define GRACTL_LATCH_TRIG	0x04	// trig[] button latching

// CONSOL: key bits are active low on read. Bit 3 controls the speaker on write.
#define CONSOL_START		0x01
#define CONSOL_SELECT		0x02
#define CONSOL_OPTION		0x04
#define CONSOL_SPEAKER		0x08

struct GTIA
{
	volatile byte	hposp[4];	// $D000 player horizontal position
	volatile byte	hposm[4];	// $D004 ball horizontal position
	volatile byte	sizep[4];	// $D008 Player width (0 = normal, 1 = double, 3 = quadruple)
	volatile byte	sizem;		// $D00C ball width, 2 bits per ball
	volatile byte	grafp[4];	// $D00D player graphics without DMA
	volatile byte	grafm;		// $D011 ball graphics without DMA
	volatile byte	colpm[4];	// $D012 player and ball colours
	volatile byte	colpf[4];	// $D016 playfield colours
	volatile byte	colbk;		// $D01A background and border colour
	volatile byte	prior;		// $D01B GTIA priorities and modes
	volatile byte	vdelay;		// $D01C vertical delay by one line
	volatile byte	gractl;		// $D01D PMG control
	volatile byte	hitclr;		// $D01E any write clears collisions
	volatile byte	consol;		// $D01F console speaker
};

struct GTIARead
{
	volatile byte	mpf[4];		// $D000 ball-playfield collisions
	volatile byte	ppf[4];		// $D004 player-playfield collisions
	volatile byte	mpl[4];		// $D008 ball-player collisions
	volatile byte	ppl[4];		// $D00C player-player collisions
	volatile byte	trig[4];	// $D010 Active-low triggers. On XL/XE, TRIG3 indicates cartridge presence.
	volatile byte	pal;		// $D014 bits 1..3: 000 = PAL, 111 = NTSC
	byte			unused[10];
	volatile byte	consol;		// $D01F START/SELECT/OPTION keys
};

#define GTIA_IS_PAL()	((gtiar.pal & 0x0e) == 0)

// Write registers
#define gtia	(*((struct GTIA *)0xd000))
// Read registers
#define gtiar	(*((struct GTIARead *)0xd000))

#endif
