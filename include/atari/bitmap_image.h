#ifndef ATARI_BITMAP_IMAGE_H
#define ATARI_BITMAP_IMAGE_H

#include "bitmap.h"
#include "cio.h"

#define ABM_IMAGE_FORMAT 200
#define ABM_IMAGE_LAYOUT 201
#define ABM_IMAGE_BUFFER 202

struct ABitmapPalette
{
	byte count;     // Number of valid colors in the array, or zero for raw MIC.
	byte colors[5]; // Background, then COLPF0 through COLPF3.
};

// Decode four-color ANTIC E images into a caller-owned bitmap. The pitch may
// include padding. MIC uses 40 bytes per row and the destination height. PIC
// validates both dimensions from its header. The palette may be null. On
// error neither pixels nor palette are written. Input and output must be separate.
byte abm_decode_mic(const ABitmap * bitmap, const byte * data, word size, ABitmapPalette * palette);
byte abm_decode_pic(const ABitmap * bitmap, const byte * data, word size, ABitmapPalette * palette);

struct ABitmapImageFile
{
	const char * name;
	byte * work;
	word capacity;
};

// Read the complete file through CIO into caller-owned scratch, then decode.
// Return CIO_OK, an ABM_IMAGE_* status, or the original CIO error.
byte abm_load_mic(const ABitmap * bitmap, const ABitmapImageFile * file, ABitmapPalette * palette);
byte abm_load_pic(const ABitmap * bitmap, const ABitmapImageFile * file, ABitmapPalette * palette);

#pragma compile("bitmap_image.c")

#endif
