#ifndef ATARI_TGI_DRIVER_H
#define ATARI_TGI_DRIVER_H

#include "types.h"

#define ATGI_CONTROL        5
#define ATGI_CLEAR          6
#define ATGI_SETVIEWPAGE    7
#define ATGI_SETDRAWPAGE    8
#define ATGI_SETCOLOR       9
#define ATGI_SETPALETTE    10
#define ATGI_GETPALETTE    11
#define ATGI_GETDEFPALETTE 12
#define ATGI_SETPIXEL      13
#define ATGI_GETPIXEL      14
#define ATGI_LINE          15
#define ATGI_BAR           16
#define ATGI_TEXTSTYLE     17
#define ATGI_OUTTEXT       18

struct ATgiDriver
{
	byte * image;
	word code_size, image_size, width, height;
	byte colors, pages;
};

struct ATgiStorage
{
	byte * file, * image;
	word file_capacity, image_capacity;
};

struct ATgiRequest
{
	word p1, p2, p3, p4;
	byte operation, a, x, y;
};

// Relocate a cc65 Atari TGI ABI 6 o65 module with no external imports.
// Supply separate writable image storage including space for the BSS.
// The driver is published only on success. Use trusted driver code.
bool atgi_load(ATgiDriver * driver, const byte * file, word size, byte * image, word capacity);
bool atgi_load_file(ATgiDriver * driver, const char * path, const ATgiStorage * memory);

// Separate the install and graphics lifetimes for the <tgi.h> interface.
// Attach a relocated image. Begin returns the driver status (0 on success).
bool atgi_attach(const ATgiDriver * driver);
byte atgi_begin(word program_end);
void atgi_end(void);
void atgi_detach(void);

// Activate one driver and reserve all live program memory below program_end.
// Keep the loaded image below that limit. Call atgi_close() before reuse.
bool atgi_open(const ATgiDriver * driver, word program_end);
void atgi_close(void);

// Call an installed TGI vector with the cc65 register and four pointer arguments.
// Supply coordinates and buffers valid for the selected driver operation.
// A, X and Y receive the result. Return the TGI status ($ff if inactive).
byte atgi_call(ATgiRequest * request);

#pragma compile("tgi_driver.c")

#endif
