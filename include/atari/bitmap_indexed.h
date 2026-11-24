#ifndef ATARI_BITMAP_INDEXED_H
#define ATARI_BITMAP_INDEXED_H

#include "bitmap_image.h"

#define ABM_INDEXED_BMP 1
#define ABM_INDEXED_PCX 2
#define ABM_INDEXED_GIF 3

#define ABM_INDEXED_CIO_WORK 256u
#define ABM_INDEXED_GIF_WORK 16384u

struct ABitmapIndexedImage
{
	const ABitmap * bitmap;
	const byte * colors; // Optional 256-entry map from source indices to bitmap colors.
	byte * work;
	word capacity;
};

// Decode indexed pictures to a bound 1-, 2- or 4-bit bitmap. Source indices
// are drawn with abm_put's low-bit masking after the optional color map.
// Keep the bitmap, scratch and input disjoint. A decode error can leave
// already decoded pixels in the bitmap. Borders and row padding are preserved.
byte abm_decode_indexed(const ABitmapIndexedImage * image, byte format, const byte * data, word size);

// Stream from CIO. Supply ABM_INDEXED_CIO_WORK bytes in addition to the
// ABM_INDEXED_GIF_WORK bytes required by both GIF entry points. BMP and PCX
// require no scratch for the memory entry point.
byte abm_load_indexed(const ABitmapIndexedImage * image, byte format, const char * name);

#pragma compile("bitmap_indexed.c")

#endif
