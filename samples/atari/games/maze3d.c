#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/input.h>
#include <atari/irq.h>
#include <string.h>
#include "../support/display_state.h"

#define WIDTH 40
#define HEIGHT 25

static const byte maze_chars[] = {
	#embed "../../resources/maze3dchars.bin"
};

static byte font[1024], frames[2][1024];
#pragma align(font, 1024)
#pragma align(frames, 1024)
static byte turn_strip[HEIGHT][128];
#pragma align(turn_strip, 4096)
static byte list[81];
#pragma align(list, 1024)
static volatile byte * rows;
static byte front;
static byte * DrawScreen;

static sbyte px = 1, py = 3, dx = 1, dy = 0;
static const byte zxdist0[] = {18, 6, 4, 3, 2, 1, 0};
static const byte zxdist1[] = {9, 5, 3, 2, 1, 0, 0};

static byte glyph(byte c)
{
	return c >= 124 ? c - 112 : c - 96;
}

static void make_font(void)
{
	for (byte ch = 96; ch <= 126; ++ch)
	{
		if (ch >= 108 && ch < 124)
			continue;
		for (byte y = 0; y < 8; ++y)
		{
			byte bits = maze_chars[8 * ch + y], pixels = 0;
			for (byte x = 0; x < 8; x += 2)
				if (bits & (0xc0 >> x))
					pixels |= 0x40 >> x;
			byte offset = glyph(ch) * 8 + y;
			font[offset] = pixels;
			font[128 + offset] = pixels << 1;
			font[256 + offset] = pixels | (pixels << 1);
		}
	}
}

static __hwinterrupt void floor_color(void)
{
	antic.wsync = 0;
	gtia.colbk = COLOR_BLACK;
}

static void make_list(void)
{
	rows = dl_blank(list, 24);
	volatile byte * end = rows;
	for (byte y = 0; y < HEIGHT; ++y)
		end = dl_lms(end, DL_MODE4 | (y == 4 ? DL_DLI : 0), frames[0] + WIDTH * y);
	dl_jvb(end, list);
}

static void show_rows(const byte * screen, byte pitch, byte left)
{
	volatile byte * row = rows;
	screen += left;
	for (byte y = 0; y < HEIGHT; ++y)
	{
		word address = (word)screen;
		row[1] = (byte)address;
		row[2] = address >> 8;
		row += 3;
		screen += pitch;
	}
}

static void screen_flip(void)
{
	front ^= 1;
	os_wait_frame();
	show_rows(frames[front], WIDTH, 0);
	DrawScreen = frames[front ^ 1];
}

static void screen_turn(bool right)
{
	for (byte y = 0; y < HEIGHT; ++y)
	{
		const byte * old = frames[front] + WIDTH * y;
		const byte * next = DrawScreen + WIDTH * y;
		memcpy(turn_strip[y], right ? old : next, WIDTH);
		memcpy(turn_strip[y] + WIDTH, right ? next : old, WIDTH);
	}
	for (byte x = 4; x < WIDTH; x += 4)
	{
		os_wait_frame();
		show_rows(&turn_strip[0][0], 128, right ? x : WIDTH - x);
	}
	screen_flip();
}

static void screen_column(byte x, byte depth, byte top, byte middle, byte bottom, byte shade)
{
	byte first = depth / 4, end = HEIGHT - depth;
	byte * p = DrawScreen + x;
	for (byte y = 0; y < HEIGHT; ++y)
	{
		byte c = 126;
		if (y >= first && y < end)
			c = y == first ? top : y + 1 == end ? bottom : middle;
		*p = 16 * shade + glyph(c);
		p += WIDTH;
	}
}

// Just to get started a mini maze
static const char maze[16][17] =
{
	"################",
	"#..........#...#",
	"#.###.####.###.#",
	"#........#.#.#.#",
	"##.#####.###.#.#",
	"#..#......##.#.#",
	"#.##.####......#",
	"#.#.....#.####.#",
	"#.#####.#....#.#",
	"#.......######.#",
	"#.##.####......#",
	"#.#..##...####.#",
	"#.##....#....#.#",
	"#.#####.######.#",
	"#..............#",
	"################",
};

// Draw the current maze using the given z/x distance array
static void maze_draw(const char * zxdist)
{
	// pick colors based on orientation
	char	cleft, cright, cfront;
	if (dx)
	{
		cfront = 1;
		if (dx < 0)
		{
			cleft = 2;
			cright = 0;
		}
		else
		{
			cleft = 0;
			cright = 2;
		}
	}
	else
	{
		cleft = cright = 1;
		cfront = dy > 0 ? 2 : 0;
	}

	// Start position of player
	sbyte	ix = px, iy = py;

	// Starting at first screen column
	sbyte	sx = 0;

	for(char i=0; i<7; i++)
	{
		// Next screen column
		sbyte	tx = 20 - zxdist[i];

		// View blocked by wall
		if (maze[iy][ix] == '#')
		{
			// Fill with wall color
			for(char cx=sx; cx<40-sx; cx++)
			{
				screen_column(cx, sx, 96 + (sx & 3), 96, 96, cfront);
			}

			// And be done
			return ;
		}

		// Check for left wall
		if (maze[iy - dx][ix + dy] == '#')
		{
			// Draw left wall
			for(char cx=sx; cx<tx; cx++)
			{

				screen_column(cx, cx, 100 + (cx & 3), 96, 124, cleft);
			}
		}
		else
		{
			// Draw adjacent wall visible due to free space left

			// All at same height, wall is facing us
			for(char cx=sx; cx<tx; cx++)
			{
				screen_column(cx, tx, 96 + (tx & 3), 96, 96, cfront);
			}
		}

		// Check for right wall
		if (maze[iy + dx][ix - dy] == '#')
		{
			for(char cx=sx; cx<tx; cx++)
			{
				screen_column(39 - cx, cx, 107 - (cx & 3), 96, 125, cright);
			}
		}
		else
		{
			for(char cx=sx; cx<tx; cx++)
			{
				screen_column(39 - cx, tx, 96 + (tx & 3), 96, 96, cfront);
			}
		}

		// Advance in maze
		sx = tx;
		ix += dx;
		iy += dy;
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte saved_chbas = os_chbas;
	void * saved_dli = os_vdslst;
	make_font();
	make_list();
	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_GRAY1;
	os_color[1] = COLOR_GRAY2;
	os_color[2] = COLOR_GRAY3;
	os_color[4] = COLOR_WHITE;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	DrawScreen = frames[0];
	maze_draw(zxdist0);
	DrawScreen = frames[1];
	dl_set(list);
	os_wait_frame();
	dli_set(floor_color);

	bool rotate = false;
	while (!(cons_keys() & CONSOL_START))
	{
		os_wait_frame();
		byte stick = joy_read(0);
		sbyte jy = ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);
		if (jy)
		{
			sbyte tx = px - dx * jy, ty = py - dy * jy;
			if (maze[ty][tx] != '#')
			{
				if (jy < 0)
				{
					px = tx;
					py = ty;
				}
				maze_draw(zxdist1);
				screen_flip();
				px = tx;
				py = ty;
				maze_draw(zxdist0);
				screen_flip();
			}
		}

		sbyte jx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
		if (jx && !rotate)
		{
			sbyte x = dx;
			dx = -dy * jx;
			dy = x * jx;
			maze_draw(zxdist0);
			screen_turn(jx > 0);
		}
		rotate = jx != 0;
	}

	dli_off();
	os_vdslst = saved_dli;
	os_chbas = saved_chbas;
	bitmap_demo_restore(&saved);
	return 0;
}
