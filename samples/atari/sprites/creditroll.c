#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/input.h>
#include <string.h>
#include "../support/display_state.h"

#define WIDTH 40
#define HEIGHT 200
#define PITCH 64
#define GLYPH_ROWS 8
#define TRACK 24
#define PAGE_ROWS 64

static const byte charset[] = {
	#embed "../../resources/scifiglyph.bin"
};

static const char * const scrolltext[] = {


"This sample uses",
"six sprites",
"multiplexed six",
"times to provide",
"twelve lines of",
"text with eighteen",
"characters each",
"",
"each character is",
"extracted from a",
"custom font into a",
"new sprite and the",
"sprites are then",
"moved up the",
"screen",
"",
"interrupts are",
"used to switch the",
"sprite vertical",
"position and",
"graphics",
"data every two",
"character lines"
"",
"",
"",
"",
""
};


#define LINES (sizeof(scrolltext) / sizeof(scrolltext[0]))
#define PAGES ((LINES * GLYPH_ROWS + PAGE_ROWS - 1) / PAGE_ROWS)
static byte art[PAGES][4096];
#pragma align(art, 4096)
static const byte * addresses[LINES][GLYPH_ROWS];
static byte list[2][1024];
#pragma align(list, 1024)
static byte empty[WIDTH];
#pragma align(empty, 64)

static const byte expanded[16] = {
	0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,
	0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff
};

static void make_art(void)
{
	for (byte line = 0; line < LINES; ++line)
	{
		const char * text = scrolltext[line];
		byte n = strlen(text), left = (18 - n) >> 1;
		for (byte y = 0; y < GLYPH_ROWS; ++y)
		{
			word i = (word)line * GLYPH_ROWS + y;
			byte * start = art[i / PAGE_ROWS] + PITCH * (i % PAGE_ROWS);
			addresses[line][y] = start;
			byte * out = start + 2 + 2 * left;
			for (byte x = 0; x < n; ++x)
			{
				byte glyph = charset[8 * (text[x] & 31) + y];
				*out++ = expanded[glyph >> 4];
				*out++ = expanded[glyph & 15];
			}
		}
	}
}

static void draw_list(byte page, int start)
{
	volatile byte * end = dl_blank(list[page], 24);
	byte line = 0, phase = 0;
	if (start >= 0)
	{
		line = start / TRACK;
		phase = start % TRACK;
	}
	for (byte y = 0; y < HEIGHT; ++y)
	{
		const byte * row = empty;
		if (start >= 0)
		{
			if (phase < 16)
				row = addresses[line][phase >> 1];
			if (++phase == TRACK)
			{
				phase = 0;
				if (++line == LINES) line = 0;
			}
		}
		++start;
		end = dl_lms(end, DL_MODEF, row);
	}
	dl_jvb(end, list[page]);
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	make_art();
	byte page = 0;
	int y = -HEIGHT;
	draw_list(page, y);
	os_color[1] = COLOR_WHITE;
	os_color[2] = os_color[4] = COLOR_BLACK;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	dl_set(list[page]);

	while (!(cons_keys() & CONSOL_START))
	{
		page ^= 1;
		if (++y == LINES * TRACK) y = 0;
		draw_list(page, y);
		os_wait_frame();
		os_sdlst = (word)list[page];
		antic.dlist = os_sdlst;
	}

	bitmap_demo_restore(&saved);
	return 0;
}
