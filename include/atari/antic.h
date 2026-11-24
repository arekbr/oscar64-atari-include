#ifndef ATARI_ANTIC_H
#define ATARI_ANTIC_H

#include "types.h"

// ANTIC, $D400..$D40F.

// DMACTL (shadow: os_sdmctl)
#define DMACTL_PF_OFF		0x00
#define DMACTL_PF_NARROW	0x01	// 32 bytes per line
#define DMACTL_PF_NORMAL	0x02	// 40 bytes per line
#define DMACTL_PF_WIDE		0x03	// 48 bytes per line
#define DMACTL_MISSILES		0x04	// Ball DMA
#define DMACTL_PLAYERS		0x08	// Player DMA
#define DMACTL_PM_1LINE		0x10	// PMG in single-line resolution (otherwise two lines)
#define DMACTL_DLIST		0x20	// ANTIC reads display list

// CHACTL (shadow: os_chact)
#define CHACTL_BLANK_INV	0x01	// Blank characters with bit 7 set.
#define CHACTL_INVERSE		0x02	// Invert characters with bit 7 set.
#define CHACTL_REFLECT		0x04	// Reflect characters vertically.

// NMIEN
#define NMIEN_RESET			0x20	// 400/800 only
#define NMIEN_VBI			0x40
#define NMIEN_DLI			0x80

// Display list instructions
#define DL_BLANK1			0x00	// 1..8 blank lines
#define DL_BLANK2			0x10
#define DL_BLANK3			0x20
#define DL_BLANK4			0x30
#define DL_BLANK5			0x40
#define DL_BLANK6			0x50
#define DL_BLANK7			0x60
#define DL_BLANK8			0x70
#define DL_JMP				0x01	// Jump, followed by 2-byte address
#define DL_JVB				0x41	// Jump and wait for VBLANK, followed by 2-byte address
#define DL_HSCROL			0x10	// Modifier: horizontal scroll
#define DL_VSCROL			0x20	// Modifier: vertical scroll
#define DL_LMS				0x40	// Load a new screen memory address from two following bytes.
#define DL_DLI				0x80	// Request a display list interrupt on the last scanline.

// ANTIC modes (OS GRAPHICS equivalent in parentheses)
#define DL_MODE2			0x02	// Text 40 col., 8 lines (GR.0)
#define DL_MODE3			0x03	// Text 40 col., 10 lines
#define DL_MODE4			0x04	// Text 4-colour, 8 lines (GR.12)
#define DL_MODE5			0x05	// Text 4-colour, 16 lines (GR.13)
#define DL_MODE6			0x06	// Text 20 col., 8 lines (GR.1)
#define DL_MODE7			0x07	// Text 20 col., 16 lines (GR.2)
#define DL_MODE8			0x08	// Graphics 40 pels, 4 colours (GR.3)
#define DL_MODE9			0x09	// Graphics 80 pels, 2 colours (GR.4)
#define DL_MODEA			0x0a	// Graphics 80 pels, 4 colours (GR.5)
#define DL_MODEB			0x0b	// Graphics 160 pels, 2 colours (GR.6)
#define DL_MODEC			0x0c	// Graphics 160 pels, 2 colours, 1 line (GR.14)
#define DL_MODED			0x0d	// Graphics 160 pels, 4 colours (GR.7)
#define DL_MODEE			0x0e	// Graphics 160 pels, 4 colours, 1 line (GR.15)
#define DL_MODEF			0x0f	// Graphics 320 pels, 1 colour (GR.8)

struct ANTIC
{
	volatile byte	dmactl;		// $D400 (Z) DMA
	volatile byte	chactl;		// $D401 (W) Character control
	volatile word	dlist;		// $D402 (W) Display list address. The low byte is written first.
								// Change it during vertical blank or with display DMA disabled.
	volatile byte	hscrol;		// $D404 (Z) horizontal scroll 0..15
	volatile byte	vscrol;		// $D405 (Z) vertical scroll 0..15
	byte			unused0;
	volatile byte	pmbase;		// $D407 (Z) PMG memory page
	byte			unused1;
	volatile byte	chbase;		// $D409 (Z) character set page
	volatile byte	wsync;		// $D40A (Z) any write halts CPU until end of line
	volatile byte	vcount;		// $D40B (O) current line / 2
	volatile byte	penh;		// $D40C (O) light pen, horizontal
	volatile byte	penv;		// $D40D (O) light pen, vertical
	volatile byte	nmien;		// $D40E (Z) NMI enable
	union
	{
		volatile byte	nmires;	// $D40F (Z) any write clears NMI state
		volatile byte	nmist;	// $D40F (O) NMI source
	};
};

#define antic	(*((struct ANTIC *)0xd400))

#endif
