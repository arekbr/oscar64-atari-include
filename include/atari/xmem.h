#ifndef ATARI_XMEM_H
#define ATARI_XMEM_H

#include "types.h"

// 130XE extended memory exposes four 16 KB banks at $4000-$7FFF through
// PORTB ($D301). Link the xmemcode section outside this window, for example:
//
//	#pragma region(xmem, 0x2080, 0x2180, , , {xmemcode})
//	#pragma region(main, 0x2180, 0xbc00, , , {code, data, bss, heap, stack})
//
// Oscar64 reports a linker error if xmemcode is left without a region.
// Detect and copy check a 256-byte span starting at xm_code, including
// its last byte. Keep the entire span outside the window.
//
// To leave the window free for direct CPU mapping, place every program
// section outside it, including the stack and heap. One possible layout:
//
//	#pragma region(main, 0x2080, 0x4000, , , {xmemcode, code, data, bss, heap, stack})
//	#pragma region(high, 0x8000, 0xbc00, , , {code, data, bss})
//
// IRQ and NMI remain enabled as configured by the caller. Masking IRQ
// during a copy would cause XL OS rev. 2 and 3 to skip deferred VBI
// processing when the interrupted code has the I flag set. Interrupt
// handlers, their callees, stacks, data and buffers must remain outside the
// window while a bank is mapped. Detect and copy can check only the
// targets of 18 OS vectors ($0200-$0217, $0222-$0229, $0236-$0239).
// Keep interrupt handlers from clobbering the module's xm_* zero-page
// state. The ROM floating-point package uses the same $E0-$FF area.
// Call the API from foreground code. It shares state between calls.
//
// PORTB bits 2-3 select the bank for both CPU and ANTIC. Clearing bit 4
// selects banked memory for the CPU. On machines with separate ANTIC
// access, clearing bit 5 selects banked memory for ANTIC. Setting bit 7
// disables self-test ROM in $5000-$57FF. Mapping preserves bits 0 (OS
// ROM), 1 (BASIC) and 6. Use the XL OS PIA configuration with DDRB = $FF
// and PBCTL bit 2 set. With that bit clear, $D301 selects DDRB and the
// API returns XMEM_E_PIA before accessing $D301.
//
// 130XE and Compy Shop 320 support independent ANTIC access. RAMBO and
// the 576 KB and 1088 KB expansions use bit 5 for bank selection. The
// API exposes four banks. On expanded machines, bits 1, 6 and 7 can
// also affect which physical banks are selected. Keep bits 1 and 6
// stable between calls. Copy and map select banks with bit 7 set. On
// CPU-only expansions they also set bit 5.

#define XMEM_BASE		0x4000
#define XMEM_SIZE		0x4000

#define XMEM_CPU		0x01
#define XMEM_ANTIC		0x02

#define XMEM_CAP_ANTIC	0x01

#define XMEM_OK			0
#define XMEM_E_BANK		1		// Bank number is outside the detected range.
#define XMEM_E_RANGE	2		// Offset exceeds the bank size, or the copy would cross its end.
#define XMEM_E_VECTOR	3		// An interrupt vector points into the window.
#define XMEM_E_LAYOUT	4		// The reserved xmemcode span overlaps the window.
#define XMEM_E_MAPPED	5		// The CPU already sees a bank in the window.
#define XMEM_E_PIA		6		// PBCTL bit 2 is clear.
#define XMEM_E_ARG		7		// The mapping flags are zero or contain unknown bits.
#define XMEM_E_ANTIC	8		// ANTIC mapping requires XMEM_CAP_ANTIC.

#pragma section(xmemcode, 0)

extern byte xmem_banks;		// 0 or 4 after detection.
extern byte xmem_caps;		// XMEM_CAP_* flags after detection.

// Clear xmem_banks and xmem_caps, then check PIA, layout, CPU mapping and
// vectors in that order. Success with xmem_banks == 0 means no supported
// banks were found. The probe checks for distinct banks, saves and
// restores the bytes it touches at $4000, and restores PORTB.
byte xmem_detect(void);

// Copy between main RAM and the selected bank. The buffer can be
// unaligned and can reside in the main RAM behind the bank window.
// Check PIA, layout, CPU mapping, bank and range in that order. A valid
// zero-length copy succeeds before the vector check. On error, leave
// the buffer and bank untouched. Preserve PORTB and the CPU I flag.
// When ANTIC already uses banked RAM, copying to or from another bank
// briefly changes the bank visible to ANTIC and can disturb the display.
byte xmem_put(byte bank, word off, const void * src, word n);
byte xmem_get(byte bank, word off, void * dst, word n);

// Map the bank for XMEM_CPU, XMEM_ANTIC or both. Check PIA, presence of
// detected banks, mapping flags, ANTIC capability and bank number in
// that order. Set PORTB to (PORTB & $43) | $80 | (bank << 2), plus $10
// if the CPU uses main RAM and $20 if ANTIC uses main RAM. For CPU
// mapping, keep all live code, data and the stack outside the window.
// The caller is responsible for layout and vectors.
byte xmem_map(byte bank, byte who);

// Restore main RAM for the CPU and ANTIC by setting PORTB bits 4 and 5.
// Works before detection and on 64 KB machines. Other PORTB bits retain
// their current values. To restore the exact mapping from before a call
// to xmem_map, save and restore PORTB through volatile __memmap access.
byte xmem_unmap(void);

#pragma compile("xmem.c")

#endif
