#ifndef ATARI_OS_H
#define ATARI_OS_H

#include "types.h"

// Atari XL/XE OS variables and register shadows.
// The OS copies shadow registers to the corresponding hardware registers
// during vertical blank. Update a shadow to keep a setting across frames.

#define os_rtclok	((volatile byte *)0x0012)		// [0..2] Frame counter, most significant byte first
#define os_pokmsk	(*(volatile byte *)0x0010)		// POKEY IRQEN shadow
#define os_atract	(*(volatile byte *)0x004d)		// blanker counter, 0 = reset
#define os_lmargn	(*(volatile byte *)0x0052)		// editor left margin
#define os_rmargn	(*(volatile byte *)0x0053)		// editor right margin
#define os_rowcrs	(*(volatile byte *)0x0054)		// cursor row
#define os_colcrs	(*(volatile word *)0x0055)		// cursor column
#define os_dindex	(*(volatile byte *)0x0057)		// Video mode number used by the OS
#define os_savmsc	(*(volatile word *)0x0058)		// memory image address
#define os_palnts	(*(volatile byte *)0x0062)		// XL/XE: 0 = NTSC, 1 = PAL (OS sets at boot from GTIA PAL)
#define os_ramtop	(*(volatile byte *)0x006a)		// RAM end page
#define os_keydef	(*(byte * volatile *)0x0079)	// XL/XE: key code -> ATASCII table (192 B)

#define os_vdslst	(*(void * volatile *)0x0200)	// DLI vector
#define os_vvblki	(*(void * volatile *)0x0222)	// immediate VBI vector
#define os_vvblkd	(*(void * volatile *)0x0224)	// delayed VBI vector

#define os_sdmctl	(*(volatile byte *)0x022f)		// ANTIC DMACTL shadow
#define os_sdlst	(*(volatile word *)0x0230)		// ANTIC DLIST shadow
#define os_gprior	(*(volatile byte *)0x026f)		// GTIA PRIOR shadow
#define os_paddl	((volatile byte *)0x0270)		// [0..7] Paddle values. 228 also indicates no paddle. XL/XE mirrors [0..3] at [4..7].
#define os_stick	((volatile byte *)0x0278)		// [0..1] joysticks, 15 = center
#define os_ptrig	((volatile byte *)0x027c)		// [0..7] Paddle triggers, active low. XL/XE mirrors [0..3] at [4..7].
#define os_strig	((volatile byte *)0x0284)		// [0..1] buttons, 0 = pressed

#define os_txtrow	(*(volatile byte *)0x0290)		// text window (GR.1+): cursor row - OS swaps with ROWCRS on context change
#define os_txtcol	(*(volatile word *)0x0291)		// text window: cursor column (from COLCRS)
#define os_tindex	(*(volatile byte *)0x0293)		// text window: GRAPHICS mode (from DINDEX), 0 for GR.0 window
#define os_txtmsc	(*(volatile word *)0x0294)		// text window: image memory address (from SAVMSC)

#define os_invflg	(*(volatile byte *)0x02b6)		// Keyboard inverse-video flag ($00 = off, $80 = on)
#define os_shflok	(*(volatile byte *)0x02be)		// Caps: 0 lowercase, $40 uppercase, $80 Control
#define os_noclik	(*(volatile byte *)0x02db)		// XL/XE: != 0 = keyboard click disabled (0 = click enabled)
#define os_helpfg	(*(volatile byte *)0x02dc)		// HELP key latch. Zero means no key pending. Clear it after reading.

#define os_pcolr	((volatile byte *)0x02c0)		// [0..3] GTIA COLPM shadows
#define os_color	((volatile byte *)0x02c4)		// [0..3] COLPF shadows, [4] = COLBK

#define os_crsinh	(*(volatile byte *)0x02f0)		// 1 = cursor invisible
#define os_chact	(*(volatile byte *)0x02f3)		// ANTIC CHACTL shadow
#define os_chbas	(*(volatile byte *)0x02f4)		// ANTIC CHBASE shadow
#define os_ch		(*(volatile byte *)0x02fc)		// key code, 255 = none
#define os_fildat	(*(volatile byte *)0x02fd)		// fill colour number for XIO 18 (FILL)
#define os_dspflg	(*(volatile byte *)0x02fe)		// Nonzero displays control characters literally in E:

// The CPU vectors normally reside in the OS ROM.
// The OS interrupt handler vectors in RAM are declared above.
// With OS ROM disabled, the macros read RAM at $FFFA-$FFFF.

#define OS_NMIVEC_ADDR		0xfffa
#define OS_RESETVEC_ADDR	0xfffc
#define OS_IRQVEC_ADDR		0xfffe
#define os_nmivec	(*(const volatile word *)OS_NMIVEC_ADDR)		// NMI vector
#define os_resetvec	(*(const volatile word *)OS_RESETVEC_ADDR)	// Reset vector
#define os_irqvec	(*(const volatile word *)OS_IRQVEC_ADDR)		// IRQ vector

// Wait for the next OS frame tick.
// The OS vertical blank handler must be active.
void os_wait_frame(void);

// Wait for n changes of the low byte of RTCLOK. Zero returns immediately.
// The first wait starts at the current point in the frame.
void os_wait_frames(word n);

#pragma compile("os.c")

#endif
