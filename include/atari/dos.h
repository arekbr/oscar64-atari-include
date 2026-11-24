#ifndef ATARI_DOS_H
#define ATARI_DOS_H

#include "cio.h"

// Enter the booted DOS through DOSVEC. This call does not return.
// Close open files and restore any interrupt handlers before calling.
// DOS and its vector must still be intact.
void dos_exit(void);

// The current directory on SpartaDOS X. Drive zero selects the default;
// 1 through 15 select D1: through DO:. The root is an empty C string.
// On error, including CIO_TRUNC if size is too small, path is unchanged.
byte dos_getcwd(byte drive, char * path, byte size);

// The current SpartaDOS X default drive, 1 through 15. Zero means unavailable.
byte dos_default_drive(void);

struct DOSArgs
{
	char text[64];
	char * argv[33];
	byte argc;
};

// Read the SpartaDOS X launch line before other DOS command-line calls.
// Spaces separate tokens. The program token is argv[0], and argv[argc]
// is null. All tokens point into args->text. On failure args is unchanged.
bool dos_args(DOSArgs * args);

#pragma compile("dos.c")

#endif
