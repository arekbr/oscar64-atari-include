#ifndef ATARI_CIO80_H
#define ATARI_CIO80_H

#include "cio.h"
#include "bitmap_screen.h"

// Register device 8 in HATABS, accessible as "80:" through the OS CIOV.
// Supply a prepared 320x192 ANTIC F display page and a 4x8 text font.
// One channel owns the display. Close the channel before removing it.
byte cio80_install(const ABitmapDisplayPage * page, const byte * font);
byte cio80_remove(void);

#pragma compile("cio80.c")

#endif
