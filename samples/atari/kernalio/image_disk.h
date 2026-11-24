#ifndef ATARI_SAMPLE_IMAGE_DISK_H
#define ATARI_SAMPLE_IMAGE_DISK_H

#include <atari/colors.h>
#include <atari/input.h>
#include "disk_demo.h"
#include "../support/bitmap_demo.h"

#define IMAGE_PATH "D:SAMPLE.MIC"
#define ROWS 200
#define ROW_BYTES 40
#define IMAGE_BYTES (ROWS * ROW_BYTES)

static word transferred;

static bool show_mono_image(void)
{
	if (!show_bitmap(DL_MODEF, ROWS))
		return false;
	os_color[1] = COLOR_WHITE;
	os_color[2] = ATARI_COLOR(HUE_YELLOW, 0);
	return true;
}

#endif
