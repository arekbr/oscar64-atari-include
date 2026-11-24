#ifndef ATARI_BITMAP_SCREEN_H
#define ATARI_BITMAP_SCREEN_H

#include "bitmap.h"

struct ABitmapPageMemory
{
	volatile byte * pixels;
	word pixel_size;
	volatile byte * list;
	word list_size;
};

struct ABitmapScreenMode
{
	byte mode, playfield, pitch, height, top;
};

struct ABitmapDisplayPage
{
	ABitmap bitmap;
	volatile byte * list;
	byte playfield;
};

// Build one display page with caller-owned storage. A rejected layout
// leaves the descriptor and display list unchanged. The bitmap is not cleared.
bool abm_page_init(ABitmapDisplayPage * page, const ABitmapPageMemory * memory, const ABitmapScreenMode * mode);

// Show a prepared page and wait for the OS VBI to latch its list. Draw
// through the bitmap of any prepared page, including the visible one.
// The caller owns GTIA priority, colors and restoration of the old screen.
void abm_page_show(const ABitmapDisplayPage * page);

struct ABitmapScreen
{
	ABitmap pages[2];
	volatile byte * lists[2];
	byte front, playfield;
};

// Build two lists in caller-owned memory. A rejected layout leaves both
// lists and the screen descriptor unchanged. Page 0 is initially in front.
// Call with both pages and lists available for exclusive use by this screen.
bool abm_screen_init(ABitmapScreen * screen, const ABitmapPageMemory * memory, const ABitmapScreenMode * mode);

// Return the page that is safe to draw into. Reacquire it after each present.
const ABitmap * abm_screen_back(const ABitmapScreen * screen);

// Display the current front page. Requires the OS vertical blank handler.
// Select GTIA 9/10/11 in the priority shadow before showing ABM_GTIA.
// The caller owns color/priority configuration and restoration of the old screen.
void abm_screen_show(const ABitmapScreen * screen);

// Present the back page and wait until the old front page can be reused.
// After show, lists with equal low bytes (for example, two page-aligned
// lists) or equal high bytes allow one swap per frame. Other list pairs
// use the two-boundary path. A swap requested across a VBI may wait for
// one extra boundary. Keep the OS display-list shadow owned by this screen.
void abm_screen_present(ABitmapScreen * screen);

#pragma compile("bitmap_screen.c")

#endif
