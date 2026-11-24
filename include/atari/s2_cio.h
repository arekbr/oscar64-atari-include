#ifndef ATARI_S2_CIO_H
#define ATARI_S2_CIO_H

#include "cio.h"

// These modes are defined by the resident S_VBXE BIOS.
#define S2_CONSOLE    0
#define S2_TEXT_80X24 1
#define S2_LOW        2
#define S2_STANDARD   3
#define S2_HIGH       4
#define S2_TEXT_80X25 5
#define S2_TEXT_80X30 6
#define S2_TEXT_80X32 7
#define S2_NOCLEAR    32
#define S2_GRAYSCALE  64

struct AS2Mode
{
	word width;
	byte height, attributes;
};

// Verify the resident S_VBXE BIOS with XIO 96 on a closed channel.
// Does not open a graphics mode. Return a CIO status code.
byte s2_cio_probe(byte channel);
byte s2_cio_open(byte channel, byte mode, byte flags, AS2Mode * result);
// As with cio_xio(), AUX1 and AUX2 receive any values returned by the BIOS.
byte s2_cio_command(byte channel, byte command, byte aux1, byte aux2);
byte s2_cio_font_size(byte channel, byte * width, byte * height);

#pragma compile("s2_cio.c")

#endif
