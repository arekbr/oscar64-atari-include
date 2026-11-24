#ifndef ATARI_HCM_H
#define ATARI_HCM_H

#include "bitmap.h"

#define HCM_COLUMNS 28
#define HCM_ROWS 25
#define HCM_SCANLINES 200
#define HCM_ROW_PROGRAM_SIZE 1172
#define HCM_PMG_ROW_PROGRAM_SIZE 2347

struct AHcm
{
	volatile byte * players, * list, * pixels;
	word list_size, pixel_size;
	byte mode;
};

// Install a narrow 200-line ANTIC E or F bitmap and a 28-column PMG
// color map. A cell covers four color clocks and eight scanlines.
// Supply a 2 KiB-aligned PMG buffer. The mode owns PMG and DLI.
// The raster handler runs throughout the visible 200 scanlines.
bool hcm_show(const AHcm * screen);

struct AHcmRowColor
{
	byte reg, value;
};

struct AHcmRows
{
	const AHcmRowColor * colors;
	volatile byte * code;
	word capacity;
};

// Supply HCM_ROWS color records and HCM_ROW_PROGRAM_SIZE bytes of
// executable RAM. Each row writes a GTIA register offset $12–$1A
// ($D012–$D01A). Offset zero leaves the GTIA colors alone. The records
// can be reused after setup. Keep the code in place until HCM stops.
bool hcm_show_rows(const AHcm * screen, const AHcmRows * program);


// Like hcm_show_rows, with a second color change at offset 4 (scanlines
// are numbered 0..7). pmg supplies one COLPM0/COLPM2 value per row.
// Provide HCM_PMG_ROW_PROGRAM_SIZE bytes of executable RAM.
bool hcm_show_pmg_rows(const AHcm * screen, const AHcmRows * program, const byte * pmg);

// Set and read a cell color index, or set a span beginning at a scanline.
// Indices 2 and 1 enable the first and second color layers respectively.
bool hcm_color(const AHcm * screen, byte column, byte row, byte color);
bool hcm_span(const AHcm * screen, byte column, byte line, byte height, byte color);
// Read the first scanline of a cell, or use an absolute scanline (0..199).
// Both return zero for an out-of-bounds position.
byte hcm_read(const AHcm * screen, byte column, byte row);
byte hcm_read_scanline(const AHcm * screen, byte column, byte line);

// Set the two PMG colors through the OS shadows.
void hcm_colors(byte first, byte second);

void hcm_off(void);

#pragma compile("hcm.c")

#endif
