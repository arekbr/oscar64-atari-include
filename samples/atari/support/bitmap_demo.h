#ifndef ATARI_SAMPLE_BITMAP_DEMO_H
#define ATARI_SAMPLE_BITMAP_DEMO_H

#include <atari/bitmap.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include "display_state.h"

// Keep program and stack below the framebuffer and BASIC disabled.
#if !defined(ATARI_SAMPLE_DISK_DEMO_H) && !defined(ATARI_SAMPLE_CUSTOM_REGION)
#pragma region(main, 0x2800, 0x7800, , , {code, data, bss, heap, stack})
#endif

static ABitmap screen;
static byte display_list[210];
#pragma align(display_list, 1024)

static bool show_bitmap(byte mode, byte rows)
{
	// 102 forty-byte rows reach $9000 exactly. ANTIC can reload there
	// without a scanline wrapping within its 4 KB addressing window.
	if (!abm_init(&screen, (volatile byte *)0x8010, 40 * rows, 40, rows, mode) ||
	    !abm_display_list(display_list, sizeof(display_list), &screen, mode, 24))
		return false;
	abm_clear(&screen, 0);
	os_color[0] = ATARI_COLOR(3, 8);
	os_color[1] = mode == DL_MODEF ? ATARI_COLOR(0, 14) : ATARI_COLOR(9, 8);
	os_color[2] = mode == DL_MODEF ? 0 : ATARI_COLOR(0, 14);
	os_color[4] = 0;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(display_list);
	return true;
}

#endif
