#ifndef ATARI_VBXE_IMAGE_H
#define ATARI_VBXE_IMAGE_H

#include "bitmap_indexed.h"
#include "vbxe.h"

#define VBXE_IMAGE_MAX_WIDTH 336u
#define VBXE_IMAGE_MAX_HEIGHT 256u
#define VBXE_IMAGE_WORK (VBXE_IMAGE_MAX_WIDTH + 3u * 256u)

struct AVbxeImage
{
	const AVbxe * board;
	unsigned long pixels;
	word pitch, width, height;
	byte palette;
	byte * work;
	word capacity;
};

// Decode ABM_INDEXED_BMP, PCX or GIF to an 8-bit VRAM surface. The caller
// owns the VRAM layout and display. Row padding and transparent GIF pixels
// are preserved. A failure can leave earlier runs in VRAM. The palette is
// programmed only after a successful decode (and close, for CIO).
// Supply VBXE_IMAGE_WORK bytes, plus ABM_INDEXED_GIF_WORK for GIF, and
// ABM_INDEXED_CIO_WORK for CIO. Keep scratch, configuration and input
// disjoint. Observe vbxe_write's MEMAC-A and serialization requirements.
byte vbxe_decode_image(const AVbxeImage * image, byte format, const byte * data, word size);
byte vbxe_load_image(const AVbxeImage * image, byte format, const char * name);

// Write an uncompressed 8-bit BMP through CIO. Pass the 256 RGB entries
// associated with the surface, or pass null for a fully tracked palette.
// Supply at least VBXE_IMAGE_MAX_WIDTH
// bytes of scratch. Opening replaces the file and a later error can leave
// a partial BMP. The CIO channel is closed in either case.
byte vbxe_save_bmp(const AVbxeImage * image, const AVbxeColor * colors, const char * name);

#pragma compile("vbxe_image.c")

#endif
