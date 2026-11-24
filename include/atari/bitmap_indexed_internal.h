#ifndef ATARI_BITMAP_INDEXED_INTERNAL_H
#define ATARI_BITMAP_INDEXED_INTERNAL_H

#include "vbxe_image.h"

struct IndexedOutput
{
	const ABitmapIndexedImage * bitmap;
	word width, height;
	const AVbxeImage * vbxe;
	byte * pixels;
	byte * palette;
	word x, y, pending, colors;
};

bool indexed_write_pixels(const AVbxeImage * image, word x, word y, const byte * pixels, word count);
byte indexed_decode(IndexedOutput * output, byte format, const byte * data, word size, byte * work);
byte indexed_load(IndexedOutput * output, byte format, const char * name, byte * work);

#endif
