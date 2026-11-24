#ifndef ATARI_VBXE_H
#define ATARI_VBXE_H

#include "types.h"

#define VBXE_VRAM_SIZE 0x80000ul
#define VBXE_MEMAC_WINDOW 0xa000u
#define VBXE_MEMAC_SIZE 0x1000u

#define VBXE_XDL_ENABLED 1
#define VBXE_EXTENDED_COLOR 2
#define VBXE_OPAQUE 4
#define VBXE_TRANSPARENT_15 8

struct AVbxeColor
{
	byte red, green, blue;
};

struct AVbxePalette
{
	AVbxeColor colors[256];
	byte known[32], bank;
};

struct AVbxe
{
	volatile byte * regs;
	byte revision;
	AVbxePalette * palette;
};

// Detect an FX core at $D600 or $D700 without changing its mapping.
bool vbxe_detect(AVbxe * board);

// VRAM transfers use the 4 KiB MEMAC-A window at $A000. The code, stack,
// descriptor and CPU buffers must stay outside that window. The previous
// mapping is restored. Serialize transfers with other users of MEMAC-A.
bool vbxe_read(const AVbxe * board, unsigned long addr, byte * dest, word size);
bool vbxe_write(const AVbxe * board, unsigned long addr, const byte * data, word size);
bool vbxe_fill(const AVbxe * board, unsigned long addr, byte value, word size);

// Program up to 256 contiguous entries in one of the four palettes.
bool vbxe_palette(const AVbxe * board, byte palette, byte first, const AVbxeColor * colors, word count);

// Optionally track one palette bank in caller-owned RAM. Attach before
// programming it through this board handle. Passing null detaches it.
// Attaching starts with all entries unknown. Direct register writes and
// resets invalidate the copy. Use the same board handle for all writers.
bool vbxe_palette_track(AVbxe * board, byte bank, AVbxePalette * state);
// Return a read-only view while the requested entries remain known and
// attached. Values are the exact RGB bytes supplied to vbxe_palette().
const AVbxeColor * vbxe_palette_entries(const AVbxe * board, byte bank, byte first, word count);

void vbxe_control(const AVbxe * board, byte control);
bool vbxe_xdl(const AVbxe * board, unsigned long list, byte control);
bool vbxe_blitter_busy(const AVbxe * board);
bool vbxe_blitter_start(const AVbxe * board, unsigned long list);

#define VBXE_BCB_SIZE 21

struct AVbxeBlit
{
	unsigned long src, dst;
	int src_pitch, dst_pitch;
	signed char src_step, dst_step;
	word width, height;
	byte and_mask, xor_mask, collision_mask, zoom, pattern, control;
};

// Encode a command using pixel counts. Control and pattern follow the FX
// blitter registers. The caller supplies writable command and VRAM storage.
bool vbxe_bcb(volatile byte * encoded, const AVbxeBlit * blit);
bool vbxe_blit(const AVbxe * board, unsigned long command, const AVbxeBlit * blit);

struct AVbxeOverlay
{
	unsigned long list, pixels;
	word pitch;
	byte height, top, palette;
};

// Install a 320-pixel wide, opaque, 8-bit overlay on the current display.
// Supply 15 bytes of VRAM for the XDL, separate from the pixel allocation.
bool vbxe_show_indexed(const AVbxe * board, const AVbxeOverlay * overlay);

#pragma compile("vbxe.c")

#endif
