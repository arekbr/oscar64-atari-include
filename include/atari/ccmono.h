#ifndef ATARI_CCMONO_H
#define ATARI_CCMONO_H
#include "cscache.h"
// Convert an 8-pixel-wide mono image into an initialized CCImage for OR
// compositing. Supply one byte per row, most significant bit on the left.
// Clear bits leave the background alone. Set bits write both color bits.
// Use a null mask and distinct source and destination storage. Height
// is 1 through 16. Phase 0 uses two columns and phases 1 through 3 use
// three. Invalid input leaves the cache unchanged.
byte cc_mono(CCImage *image,const byte *rows,byte height,byte phase);
#pragma compile("ccmono.c")
#endif
