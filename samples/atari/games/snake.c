#include <atari/colors.h>
#include <atari/input.h>
#include <stdlib.h>
#include <string.h>
#include "../support/bitmap_demo.h"

#define BLANK 0
#define HEAD 1
#define BODY 0x82
#define WALL 3
#define FRUIT 4

static volatile byte cells[1000];
#pragma align(cells, 1024)
static byte font[1024];
#pragma align(font, 1024)

// Position/Direction on screen
struct Point
{
	sbyte	x, y;
};

struct Snake
{
	Point	head;		// Position of head
	Point	dir;		// Direction of head
	Point	tail[256];	// Position of tail
	byte	length;		// Length of tail
	byte	pos;		// Tail start
};

enum GameState
{
	GS_READY,		// Getting ready
	GS_PLAYING,		// Playing the game
	GS_COLLIDE		// Collided with something
};

// State of the game
struct Game
{
	GameState	state;
	byte		count;
	Snake		snake;	// use an array for multiplayer

}	TheGame;	// Only one game, so global variable


static inline void screen_put(byte x, byte y, byte ch)
{
	cells[40 * y + x] = ch;
}

static inline byte screen_get(byte x, byte y)
{
	return cells[40 * y + x];
}

static void make_font(void)
{
	static const byte head[8] = {0, 0x14, 0x55, 0x55, 0x55, 0x55, 0x14, 0};
	static const byte heart[8] = {0, 0xcc, 0xff, 0xff, 0xff, 0x3c, 0x3c, 0,};
	for (byte y = 0; y < 8; ++y)
	{
		font[8 + y] = head[y];
		font[16 + y] = head[y] | (head[y] << 1);
		font[24 + y] = 0xaa;
		font[32 + y] = heart[y];
	}
}

// Put a fruit/heart at random position
void screen_fruit(void)
{
	byte x, y;
	do
	{
		// Draw a random position
		x = 1 + rand() % 38;
		y = 1 + rand() % 23;

		// Ensure it is an empty place
	} while (screen_get(x, y) != BLANK);

	// Put the heart on screen
	screen_put(x, y, FRUIT);

}

// Clear screen and draw borders
void screen_init(void)
{
	// Fill screen with spaces
	memset((void *)cells, BLANK, sizeof(cells));

	// Bottom and top row
	for(byte x=0; x<40; x++)
	{
		screen_put(x,  0, WALL);
		screen_put(x, 24, WALL);
	}

	// Left and right column
	for(byte y=0; y<25; y++)
	{
		screen_put( 0,  y, WALL);
		screen_put( 39, y, WALL);
	}

}

// Initialize a snake
void snake_init(Snake * s)
{
	// Length of tail is one
	s->length = 1;
	s->pos = 0;

	// Snake in the center of screen
	s->head.x = 20;
	s->head.y = 12;

	// Starting to the right
	s->dir.x = 1;
	s->dir.y = 0;

	// Show head
	screen_put(s->head.x, s->head.y, HEAD);
}

bool snake_advance(Snake * s)
{
	// Promote head to start of tail
	s->tail[s->pos] = s->head;
	s->pos++;

	screen_put(s->head.x, s->head.y, BODY);

	// Advance head
	s->head.x += s->dir.x;
	s->head.y += s->dir.y;

	// Get character at new head position
	char ch = screen_get(s->head.x, s->head.y);

	// Draw head
	screen_put(s->head.x, s->head.y, HEAD);

	// Clear tail
	char	tpos = s->pos - s->length;
	screen_put(s->tail[tpos].x, s->tail[tpos].y, BLANK);

	// Did snake collect the fruit
	if (ch == FRUIT)
	{
		// Extend tail
		s->length++;
		screen_fruit();
	}
	else if (ch != BLANK)
	{
		// Snake collided with something
		return true;
	}

	return false;
}

// Change snake direction based on user input
void snake_control(Snake * s, sbyte jx, sbyte jy)
{
	// First change from horizontal to vertical, otherwise
	// check vertical to horizontal
	if (s->dir.x && jy)
	{
		s->dir.x = 0;
		s->dir.y = jy;
	}
	else if (s->dir.y && jx)
	{
		s->dir.y = 0;
		s->dir.x = jx;
	}
}

void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	switch(state)
	{
	case GS_READY:
		// Clear the screen
		screen_init();
		TheGame.count = 32;
		break;

	case GS_PLAYING:
		// Init the snake
		os_color[3] = COLOR_LIGHTBLUE;
		snake_init(&TheGame.snake);

		// Initial fruit
		screen_fruit();

		TheGame.count = 16;
		break;

	case GS_COLLIDE:
		TheGame.count = 16;
		break;
	}
}

// Colors for collision "animation"
static const byte FlashColors[] = {
	COLOR_YELLOW,
	COLOR_WHITE,
	COLOR_GRAY3,
	COLOR_YELLOW,
	ATARI_COLOR(2, 10),
	COLOR_RED,
	COLOR_GRAY2,
	COLOR_GRAY1
};

// Main game loop, invoked every vsync
void game_loop(void)
{
	switch (TheGame.state)
	{
	case GS_READY:
		// Countdown ready to start
		if (!--TheGame.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:
	{
		// Check player input on every frame
		byte stick = joy_read(0);
		sbyte dx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
		sbyte dy = ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);
		snake_control(&TheGame.snake, dx, dy);

		if (!--TheGame.count)
		{
			// Move snake every four frames, advance to collision
			// state if collided
			if (snake_advance(&TheGame.snake))
				game_state(GS_COLLIDE);
			else
				TheGame.count = 4;
		}
		break;
	}
	case GS_COLLIDE:
		// Flash the collided snake
		os_color[3] = FlashColors[(16 - TheGame.count) / 2];
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}

}



int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte saved_chbase = os_chbas;
	make_font();

	volatile byte * end = dl_blank(display_list, 24);
	end = dl_lms(end, DL_MODE4, cells);
	end = dl_mode(end, DL_MODE4, 24);
	dl_jvb(end, display_list);
	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_WHITE;
	os_color[1] = COLOR_GRAY3;
	os_color[2] = COLOR_YELLOW;
	os_color[4] = COLOR_BLACK;
	os_gprior = 0;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	game_state(GS_READY);
	dl_set(display_list);

	while (!(cons_keys() & CONSOL_START))
	{
		game_loop();
		os_wait_frame();
	}

	os_chbas = saved_chbase;
	bitmap_demo_restore(&saved);
	return 0;
}
