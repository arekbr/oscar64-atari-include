#ifndef ATARI_CSCACHE_H
#define ATARI_CSCACHE_H
#include "types.h"
// Use the same CC_MAX_ACTORS in every source file. Fewer actors leave
// more font slots for the background.
#ifndef CC_MAX_ACTORS
#define CC_MAX_ACTORS 8
#endif
#if CC_MAX_ACTORS < 1 || CC_MAX_ACTORS > 8
#error "CC_MAX_ACTORS must be 1 through 8 for the entire compilation"
#endif
#define CC_DYNAMIC_CODES (3 * CC_MAX_ACTORS)
#define CC_STATIC_LIMIT (128 - 2 * CC_DYNAMIC_CODES)
#define CC_HISTORY_ENTRIES (9 * CC_MAX_ACTORS)
// Compositor for a 40-by-25 ANTIC 4 field. Character row r uses font
// bank r & 3. Each bank is 1 KB and glyph 0 must be all zeroes.
// Call from one foreground context. The caller supplies 4 KB of fonts
// aligned to 1 KB, two 1 KB screen buffers, two histories of
// CC_HISTORY_ENTRIES entries, a 256-byte glyph lookup table and images.
// Keep native compositor code in writable RAM. The caller selects the
// displayed screen and arranges font-bank changes.
typedef struct CCEntry { volatile byte *cell; byte old; } CCEntry;
typedef struct CCHistory { CCEntry *entries; byte count; } CCHistory;
typedef void (*CCPoll)(void);
typedef struct CCState {
 volatile byte *fonts,*screens[2];
 CCHistory history[2];
 byte *glyphs;
 CCPoll poll;
 byte back,ids;
} CCState;
typedef struct CCConfig {
 volatile byte *fonts,*screen0,*screen1;
 const byte *background;
 CCEntry *history0,*history1;
 byte *glyphs;
} CCConfig;
// Each cached column has 32 bytes: seven transparent rows, up to 16
// image rows, and transparent padding. Keep data and mask stable while
// drawing. A masked pixel with data 0 and mask 0 paints opaque black.
// A null mask selects OR compositing with already translated source bits.
typedef struct CCImage { byte *data,*mask; byte cols,height,phase,capacity; byte lo[3],hi[3]; } CCImage;
typedef struct CCSource {
 const byte *raw; // Two packed 2-bit bytes per 8-pixel row.
 const byte *colors,*masks; // Two 256-byte translation tables for masked drawing.
 byte height,phase;
} CCSource;
#define CC_OK 0
#define CC_BAD 1
#define CC_FULL 2
// Make tables for source values 0 (transparent), 1 (black), 2 (own),
// and 3 (skin). The last two arguments are destination 2-bit colors.
void cc_luts(byte *colors,byte *masks,byte skin,byte own);
// Validate the font base and copy the 1000-cell background to both screens.
bool cc_init(CCState *c,const CCConfig *config);
// Select the next back screen and restore its prior actor cells.
void cc_begin(CCState *c,byte back);
// Patch static cells in the back screen before drawing any actors.
byte cc_patch(CCState *c,const word *indices,const byte *codes,byte count);
// Set data and the optional mask to buffers of at least capacity * 32 bytes.
// Initialize a cache with two or three columns. Translation replaces its
// image rows and updates the opaque bounds.
byte cc_cache_init(CCImage *image,byte capacity);
byte cc_translate(CCImage *image,const CCSource *source);
// Draw one actor per ID after cc_begin, in painter order. X is measured
// in 2-bit pixels and Y in scan lines. The image phase must match x & 3.
// Static codes are below CC_STATIC_LIMIT. Each screen has a separate
// range of CC_DYNAMIC_CODES slots within each font bank. Draws are clipped
// to the field. An off-screen draw still consumes its ID.
byte cc_draw(CCState *c,byte id,const CCImage *image,int x,int y);
#pragma compile("cscache.c")
#endif
