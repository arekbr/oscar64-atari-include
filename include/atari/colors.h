#ifndef ATARI_COLORS_H
#define ATARI_COLORS_H

#include "gtia.h"

// Named Atari color values and hue codes. Include this header explicitly.
// ATARI_COLOR(hue, lum) uses hue 0 through 15 and even luminance 0 through 14.
// Displayed hues vary with the video standard and palette.
//
// Use a numeric hue with ATARI_COLOR for the rest of the palette.
#define HUE_GREY		0
#define HUE_REDORANGE	3		// COLOR_RED
#define HUE_PURPLE		6		// COLOR_VIOLET / COLOR_PURPLE
#define HUE_BLUE		7
#define HUE_CYAN		9
#define HUE_GREEN		12
#define HUE_YELLOW		14

#define COLOR_BLACK			ATARI_COLOR(HUE_GREY, 0)
#define COLOR_GRAY1			ATARI_COLOR(HUE_GREY, 4)
#define COLOR_GRAY2			ATARI_COLOR(HUE_GREY, 6)
#define COLOR_GRAY3			ATARI_COLOR(HUE_GREY, 10)
#define COLOR_WHITE			ATARI_COLOR(HUE_GREY, 14)
#define COLOR_RED			ATARI_COLOR(HUE_REDORANGE, 2)
#define COLOR_LIGHTRED		ATARI_COLOR(HUE_REDORANGE, 12)
#define COLOR_CYAN			ATARI_COLOR(HUE_CYAN, 6)
#define COLOR_VIOLET		ATARI_COLOR(HUE_PURPLE, 8)
#define COLOR_PURPLE		COLOR_VIOLET		// Alternate name for violet.
#define COLOR_GREEN			ATARI_COLOR(HUE_GREEN, 4)
#define COLOR_LIGHTGREEN	ATARI_COLOR(HUE_GREEN, 12)
#define COLOR_BLUE			ATARI_COLOR(HUE_BLUE, 4)
#define COLOR_LIGHTBLUE		ATARI_COLOR(HUE_BLUE, 12)
#define COLOR_YELLOW		ATARI_COLOR(HUE_YELLOW, 14)
#define COLOR_BROWN			ATARI_COLOR(HUE_YELLOW, 4)

#endif
