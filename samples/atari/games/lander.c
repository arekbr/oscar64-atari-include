#include <atari/colors.h>
#include <atari/input.h>
#include <atari/pmg.h>
#include <stdlib.h>
#include <string.h>
#include "../support/bitmap_demo.h"

#define EMPTY 0
#define STAR 1
#define GROUND 2
#define PAD 3
#define PAD_TOP 4

static const byte landersprites[] = {
	#embed "../../resources/landersprites.bin"
};

static volatile byte screen_cells[1000];
#pragma align(screen_cells, 1024)
static byte font[1024];
#pragma align(font, 1024)
static byte players[2048];
#pragma align(players, 2048)
static byte player_shapes[3][21];

struct Lander
{
	float	px, py, vx, vy;
};

enum GameState
{
	GS_READY,		// Getting ready
	GS_PLAYING,		// Playing the game
	GS_LANDED,		// Landed on pad
	GS_COLLIDE		// Collided with something
};

// State of the game
struct Game
{
	GameState	state;
	byte		count;
	Lander		lander;	// use an array for multiplayer

}	TheGame;	// Only one game, so global variable

// Put one  char on screen
inline void screen_put(byte x, byte y, byte ch)
{
	__assume(y < 25);

	screen_cells[40 * y + x] = ch;
}

// Get one char from screen
inline char screen_get(byte x, byte y)
{
	__assume(y < 25);

	return screen_cells[40 * y + x];
}

void screen_init(void)
{
	// Fill screen with spaces
	memset((void *)screen_cells, EMPTY, sizeof(screen_cells));

	for(char i=0; i<100; i++)
		{
		byte x = rand() % 40;
		byte y = rand() % 25;
		screen_put(x, y, STAR);
	}

	sbyte	height[41];
	for(char i=0; i<41; i+=8)
		height[i] = 4 + rand() % 16;

	for(char step = 8; step > 1; step /= 2)
	{
		for(char i=0; i<40; i+=step)
		{
			char	p = (height[i] + height[i + step]) >> 1;
			p += rand() % step - (step / 2);
			height[i + step / 2] = p;
		}
	}

	char xp = 2 + rand() % 33;
	char yp = height[xp];
	for(char i=1; i<4; i++)
		if (height[xp + i] < yp)
			yp = height[xp + i];

	for(char i=0; i<4; i++)
		height[xp + i] = yp;

	for(char x=0; x<40; x++)
	{
		char h = height[x];
		for(char y=0; y<h; y++)
			screen_put(x, 24 - y, GROUND);
	}


	for(char i=0; i<4; i++)
	{
		screen_put(xp + i, 24 - yp, PAD);
		screen_put(xp + i, 23 - yp, PAD_TOP);
	}

}

static void make_graphics(void)
{
	font[8 + 6] = 0x10;
	font[8 + 7] = 0x10;
	for (byte y = 0; y < 8; ++y)
	{
		font[16 + y] = 0xff;
		font[24 + y] = y & 1 ? 0xaa : 0x88;
	}
	font[32 + 7] = 0x55;

	// Keep the central eight color clocks of the ship and the exhaust.
	for (byte y = 0; y < 21; ++y)
	{
		byte low = landersprites[64 + 3 * y];
		byte middle = landersprites[65 + 3 * y];
		byte high = landersprites[66 + 3 * y];
		for (byte x = 0; x < 8; ++x)
		{
			byte pair = x < 2 ? (low >> (2 - 2 * x)) & 3 : x < 6 ?
			            (middle >> (10 - 2 * x)) & 3 : (high >> (18 - 2 * x)) & 3;
			if (pair == 1)
				player_shapes[0][y] |= 0x80 >> x;
			else if (pair)
				player_shapes[1][y] |= 0x80 >> x;
		}
		player_shapes[2][y] = landersprites[128 + 3 * y + 1];
	}
}

static const byte exhaust_color[] = {COLOR_YELLOW, COLOR_WHITE, ATARI_COLOR(2, 10), COLOR_LIGHTBLUE};

static void lander_show(const Lander * lander, sbyte jy)
{
	int x = (int)lander->px;
	int y = (int)lander->py;
	PMGShape shape;
	shape.count = x >= 0 && x < 354 && y >= 18 && y < 238 ? 21 : 0;
	shape.x = 38 + (x >> 1);
	shape.y = y - 18;
	shape.scanlines = player_shapes[0];
	pmg_move_player(0, &shape);
	shape.scanlines = player_shapes[1];
	pmg_move_player(1, &shape);
	if (jy < 0 && y < 218)
		os_pcolr[2] = exhaust_color[rand() & 3];
	else
		shape.count = 0;
	shape.y += 20;
	shape.scanlines = player_shapes[2];
	pmg_move_player(2, &shape);
}

static void lander_init(Lander * lander)
{
	lander->px = 160;
	lander->py = 50;
	lander->vx = 0;
	lander->vy = 0;
	os_pcolr[0] = COLOR_GRAY1;
	lander_show(lander, 0);
}

static void lander_move(Lander * lander, sbyte jx, sbyte jy)
{
	lander->px += lander->vx;
	lander->py += lander->vy;
	lander->vx += jx * 0.02;
	lander->vy += jy * 0.1 + 0.01;
}

enum LanderCollision
{
	LCOL_FREE,
	LCOL_GROUND,
	LCOL_PAD
};

LanderCollision lander_check(Lander * lander)
{
	sbyte	ix = (sbyte)((lander->px - 24) * 0.125);
	sbyte	iy = (sbyte)((lander->py - 29) * 0.125);

	if (iy > 24)
		return LCOL_GROUND;
	if (iy < 0)
		return LCOL_FREE;

	LanderCollision	col = LCOL_FREE;
	for(char i=0; i<4; i++)
	{
		if (ix >= 0 && ix < 40)
		{
			char ch = screen_get(ix, iy);
			if (ch == GROUND)
				return LCOL_GROUND;
			else if (ch == PAD)
				col = LCOL_PAD;
		}
		ix++;
	}

	return col;
}

void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	switch(state)
	{
	case GS_READY:
		// Clear the screen
		lander_init(&TheGame.lander);
		screen_init();
		TheGame.count = 32;
		break;

	case GS_PLAYING:
		break;

	case GS_LANDED:
		TheGame.lander.py = (sbyte)((TheGame.lander.py - 29) * 0.125) * 8 + 28;
		lander_show(&TheGame.lander, 0);
		TheGame.count = 16;
		break;

	case GS_COLLIDE:
		TheGame.count = 32;
		lander_show(&TheGame.lander, 0);
		break;
	}
}

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
		lander_move(&TheGame.lander, dx, dy);
		LanderCollision col = lander_check(&TheGame.lander);
		if (col == LCOL_GROUND)
			game_state(GS_COLLIDE);
		else if (col == LCOL_PAD)
			game_state(GS_LANDED);
		else
			lander_show(&TheGame.lander, dy);
	}	break;
	case GS_LANDED:
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	case GS_COLLIDE:
		os_pcolr[0] = (rand() & 1) ? COLOR_WHITE : COLOR_BLACK;
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}

}



int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte saved_chbase = os_chbas, saved_colors[3];
	for (byte i = 0; i < 3; ++i)
		saved_colors[i] = os_pcolr[i];

	make_graphics();
	volatile byte * end = dl_blank(display_list, 24);
	end = dl_lms(end, DL_MODE4, screen_cells);
	end = dl_mode(end, DL_MODE4, 24);
	dl_jvb(end, display_list);
	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_WHITE;
	os_color[1] = COLOR_GRAY2;
	os_color[2] = COLOR_YELLOW;
	os_color[4] = COLOR_BLACK;
	os_gprior = PRIOR_P0123_PF0123;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	pmg_init(players, PMG_1LINE);
	for (byte i = 0; i < 3; ++i)
		gtia.sizep[i] = 0;
	gtia.vdelay = 0;
	os_pcolr[1] = COLOR_GRAY3;
	game_state(GS_READY);
	dl_set(display_list);

	while (!(cons_keys() & CONSOL_START))
	{
		game_loop();
		os_wait_frame();
	}

	pmg_off();
	os_chbas = saved_chbase;
	for (byte i = 0; i < 3; ++i)
		os_pcolr[i] = saved_colors[i];
	bitmap_demo_restore(&saved);
	return 0;
}
