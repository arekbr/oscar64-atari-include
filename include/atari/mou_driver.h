#ifndef ATARI_MOU_DRIVER_H
#define ATARI_MOU_DRIVER_H

#include "types.h"

#define AMOU_LEFT  0x10
#define AMOU_RIGHT 0x01

struct AMouDriver
{
	byte * image;
	word code_size, initialized_size, image_size;
};

struct AMouStorage
{
	byte * file, * image;
	word file_capacity, image_capacity;
};

struct AMouSound
{
	byte audctl, audf2, audc2;
};

struct AMouInfo
{
	word x, y;
	byte buttons;
};

struct AMouBox
{
	word left, top, right, bottom;
};

// Relocate a trusted Atari or Atari XL cc65 mouse ABI 6 small-model o65 file
// without imports. Supply disjoint buffers and keep the image in normal RAM.
bool amou_load(AMouDriver * driver, const byte * file, word size, byte * image, word capacity);
bool amou_load_file(AMouDriver * driver, const char * path, const AMouStorage * memory);

// Use port 1 as specified by the modules. Reserve POKEY channel and timer 2
// and supply the previous write-only AUDCTL/AUDF2/AUDC2 values. INSTALL can
// restart all POKEY counters. Keep the descriptor and image live until close.
bool amou_open(const AMouDriver * driver, const AMouSound * sound);
void amou_close(void);

// Call once per frame in the foreground after vertical blank, then read INFO.
// Render the cursor in the application. These functions can share a program
// with Atari TGI when all foreground calls are serialized.
bool amou_poll(void);
bool amou_info(AMouInfo * info);
bool amou_getbox(AMouBox * box);
bool amou_setbox(const AMouBox * box);
bool amou_move(int x, int y);

#pragma compile("mou_driver.c")

#endif
