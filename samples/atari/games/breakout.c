#include <atari/colors.h>
#include <atari/dlist.h>
#include <atari/input.h>
#include <atari/irq.h>
#include <atari/pmg.h>
#include <atari/sound.h>
#include <string.h>

// Atari version of the Oscar64 C64 Breakout sample.
// The playfield and collision tests use the original 320 by 200 coordinates.
#define WIDTH 40
#define HEIGHT 25
#define SCREEN_LEFT 48
#define SCREEN_TOP 32
#define WALL_COLOR 0xee
#define SHADOW_COLOR 0x04
#define PADDLE_Y 190
#define FLASH 64
#define EMPTY_TILE 96
#define BRICK_TILE 128
#define STATUS_LETTERS 32
#define STATUS_DIGITS 48

static const byte charset[] = {
	#embed "../../resources/breakoutchars.bin"
};

static const byte spriteset[] = {
	#embed "../../resources/breakoutsprites.bin"
};

static volatile byte screen[WIDTH * HEIGHT];
#pragma align(screen, 1024)
static byte font[1024];
#pragma align(font, 1024)
static byte display_list[33];
#pragma align(display_list, 1024)
static byte players[2048];
#pragma align(players, 2048)

static const byte brick_colors[] = {
	COLOR_BLACK, COLOR_WHITE, ATARI_COLOR(2, 6), COLOR_CYAN,
	COLOR_PURPLE, ATARI_COLOR(12, 8), ATARI_COLOR(7, 8), COLOR_YELLOW
};

// These are the glyph numbers in the original C64 font.
static const byte status_glyphs[] = {
	32, 83, 3, 15, 18, 5, 58, 72, 9, 7, 8, 76, 22, 19
};
static const byte status_text[] = {
	32, 33, 34, 35, 36, 37, 38, 32, 48, 48, 48, 48, 48, 48, 32, 32,
	39, 40, 41, 42, 38, 32, 48, 48, 48, 48, 48, 48, 32, 32, 32,
	43, 40, 44, 37, 45, 38, 32, 48, 32
};

static byte ball_shape[6];
static byte paddle_shape[7];

static void make_font(void)
{
	// Mode 4 uses the high bit to select COLPF3 for the brick ink.
	for (byte ch = 0; ch < 24; ++ch)
	{
		for (byte row = 0; row < 8; ++row)
		{
			byte pixels = charset[(BRICK_TILE + ch) * 8 + row];
			font[ch * 8 + row] = pixels;
			font[(FLASH + ch) * 8 + row] = pixels & ~((pixels & 0x55) << 1);
		}
	}
	for (byte ch = 0; ch < sizeof(status_glyphs); ++ch)
		memcpy(font + (STATUS_LETTERS + ch) * 8, charset + status_glyphs[ch] * 8, 8);
	memcpy(font + STATUS_DIGITS * 8, charset + STATUS_DIGITS * 8, 80);
	memcpy(font + EMPTY_TILE * 8, charset + EMPTY_TILE * 8, 64);
}

static void make_sprites(void)
{
	// Combine the original sprite and outline into one GTIA player each.
	for (byte row = 0; row < sizeof(ball_shape); ++row)
	{
		byte bits = spriteset[3 * row] | spriteset[64 + 3 * row];
		for (byte x = 0; x < 3; ++x)
		{
			if (bits & (0xc0 >> (2 * x)))
				ball_shape[row] |= 0x80 >> x;
		}
	}
	for (byte row = 0; row < sizeof(paddle_shape); ++row)
	{
		for (byte x = 0; x < 3; ++x)
		{
			byte bits = spriteset[128 + 3 * row + x] | spriteset[192 + 3 * row + x];
			if (bits & 0xf0)
				paddle_shape[row] |= 0x80 >> (2 * x);
			if (bits & 0x0f)
				paddle_shape[row] |= 0x40 >> (2 * x);
		}
	}
}

// VCOUNT advances in pairs of scan lines. A brick band spans eight counts.
__asm set_brick_color
{
		pha
		txa
		pha
		cld
		lda $d40b
		sec
		sbc #((SCREEN_TOP + 7) / 2)
		lsr
		lsr
		lsr
		and #7
		tax
		sta $d40a
		lda #SHADOW_COLOR
		sta $d017
		lda #WALL_COLOR
		sta $d018
		lda brick_colors,x
		sta $d019
		pla
		tax
		pla
		rti
}

static void make_display(void)
{
	volatile byte * end = dl_blank(display_list, 24);
	end = dl_lms(end, DL_MODE2 | DL_DLI, screen);
	for (byte row = 1; row < HEIGHT; ++row)
		*end++ = DL_MODE4 | ((row <= 14 && !(row & 1)) ? DL_DLI : 0);
	dl_jvb(end, display_list);
}

static unsigned flashingBricks[32];
static char numFlashingBricks[4];

// Increment the score from a given digit on
static void score_inc(char digit, unsigned val)
{
	// Lowest digit to increment
	char	at = 13 - digit;

	// Loop while there is still score to account for
	while (val)
	{
		// Increment one character
		char	ch = screen[at] + val % 10;

		// Remove low digit from number
		val /= 10;

		// Check overflow
		if (ch > (STATUS_DIGITS + 9))
		{
			ch -= 10;
			val++;
		}

		screen[at] = ch;

		// Next higher character
		at --;
	}
}

static void brick_put(char x, char y, char b)
{
	volatile byte * sp = screen + WIDTH * y + x;

	sp[ 0] = b | 0;
	sp[ 1] = b | 1;
	sp[ 2] = b | 2;

	sp[40] = b | 3;
	sp[41] = b | 4;
	sp[42] = b | 5;
	sp[43] = 103 - (x & 1) - 2 * (y & 1);

	sp[81] =  101 - (x & 1) + 2 * (y & 1);
	sp[82] =  100 + (x & 1) + 2 * (y & 1);
	sp[83] =  101 - (x & 1) + 2 * (y & 1);
}

static void brick_init(void)
{
	for(char y=0; y<HEIGHT; y++)
	{
		for(char x=0; x<WIDTH; x++)
		{
			screen[WIDTH * y + x] = EMPTY_TILE + (x & 1) + 2 * (y & 1);
		}
	}

	for (byte x = 0; x < WIDTH; ++x)
		screen[x] = status_text[x];

	for(char y=0; y<8; y++)
	{
		for(char x=0; x<12; x++)
		{
			brick_put(3 * x + 2, 2 * y + 1, BRICK_TILE + (x / 4) * 8);
		}
	}
}

static void brick_hit(char x, char y)
{
	__assume(x < WIDTH && y < HEIGHT);

	char c = screen[WIDTH * y + x + 0];

	if (c >= 128)
	{
		c &= 7;

		if (c >= 3)
		{
			y --;
			c -= 3;
		}

		x -= c;

		unsigned	bi = 256 * y + x;
		char i = 0, n = numFlashingBricks[3];
		while (i < n && flashingBricks[i] != bi)
			i++;

		if (i == n)
		{
			flashingBricks[i] = bi;
			numFlashingBricks[3]++;

			volatile byte * sp = screen + WIDTH * y + x;
			for (byte col = 0; col < 3; ++col)
			{
				sp[col] |= FLASH;
				sp[WIDTH + col] |= FLASH;
			}

			score_inc(0, 5);
		}
	}
}

static void brick_animate(void)
{
	char	n = numFlashingBricks[0];

	for(char i=0; i<n; i++)
	{
		char x = flashingBricks[i] & 0xff;
		char y = flashingBricks[i] >> 8;

		volatile byte * sp = screen + WIDTH * (y - 1) + (x - 1);

		char ch = 96 + (x & 1) + 2 * (y & 1);
		if (sp[ 0] >= 128)
			sp[41] = ch | 4;
		else
			sp[41] = ch;

		if (sp[ 1] >= 128)
			sp[42] = (ch ^ 1) | 4;
		else
			sp[42] = (ch ^ 1);

		if (sp[ 2] >= 128)
			sp[43] = ch | 4;
		else
			sp[43] = ch;

		if (sp[40] >= 128)
			sp[81] = (ch ^ 2) | 4;
		else
			sp[81] = (ch ^ 2);

		sp[82] = (ch ^ 3);
		sp[83] = (ch ^ 2);

		if (sp[84] < 128)
			sp[84] = (ch ^ 3);

		if (sp[122] < 128)
			sp[122] = (ch ^ 1);
		if (sp[123] < 128)
			sp[123] = ch;
		if (sp[124] < 128)
			sp[124] = (ch ^ 1);
	}

	for(char i=0; i<3; i++)
		numFlashingBricks[i] = numFlashingBricks[i + 1] - n;
	numFlashingBricks[3] = numFlashingBricks[2];

	for(char i=0; i<numFlashingBricks[3]; i++)
		flashingBricks[i] = flashingBricks[i + n];
}

struct Ball
{
	char	index;
	bool	active;
	int		sx, sy, vx, vy;
};

// using 10.6 bit fixed point math

#define BALL_INT(x)			((x) >> 6)
#define BALL_COORD(x, f)	(((x) * 64) + f)

static void ball_init(Ball * ball, char index, int sx, int sy, int vx, int vy)
{
	ball->index = index;
	ball->active = true;
	ball->sx = sx;
	ball->sy = sy;
	ball->vx = vx;
	ball->vy = vy;
}

static void ball_move(const Ball * ball)
{
	PMGShape shape;
	shape.scanlines = ball_shape;
	shape.count = ball->active ? sizeof(ball_shape) : 0;
	shape.x = SCREEN_LEFT + (BALL_INT(ball->sx) >> 1);
	shape.y = SCREEN_TOP + BALL_INT(ball->sy);
	pmg_move_player(ball->index + 1, &shape);
}

#define	COL_00	1
#define	COL_01	2
#define	COL_10	4
#define	COL_11	8

static int paddlex, paddlevx;

// POKEY dividers for the three impact notes with the 15 kHz clock.
#define WALL_NOTE 113
#define PADDLE_NOTE 56
#define BRICK_NOTE 28

static byte sound_frames;

static void sound_trigger(byte divider)
{
	sound(0, divider, SOUND_PURE, 10);
	sound_frames = 8;
}

static void sound_loop(void)
{
	if (sound_frames)
		pokey.aud[0].audc = 0xa0 | --sound_frames;
}

static void ball_loop(Ball * ball)
{
	if (!ball->active)
		return;

	ball->sx += ball->vx;
	ball->sy += ball->vy;

	bool	mirrorX = false, mirrorY = false;

	int	ix = BALL_INT(ball->sx), iy = BALL_INT(ball->sy);
	int	px = BALL_INT(paddlex);

	if (ix + 6 > 320 || ix < 0)
	{
		mirrorX = true;
		sound_trigger(WALL_NOTE);
	}

	if (iy < 0)
	{
		mirrorY = true;
		sound_trigger(WALL_NOTE);
	}
	else if (iy >= 200)
	{
		ball->active = false;
		return;
	}

	if (iy + 6 > PADDLE_Y && iy < PADDLE_Y)
	{
		if (ix + 5 >= px && ix < px + 48)
		{
			mirrorY = true;
			if (ix < px && ball->vx > 0)
			{
				mirrorX = true;
			}
			else if (ix + 6 >= px + 48 && ball->vx < 0)
			{
				mirrorX = true;
			}
			else
			{
				ball->vx = (ball->vx * 6 + paddlevx + 4) >> 3;
			}

			sound_trigger(PADDLE_NOTE);
		}
	}


	int	x0 = ix >> 3;
	int y0 = iy >> 3;
	int	x1 = (ix + 5) >> 3;
	int y1 = (iy + 5) >> 3;


	if (x0 >= 0 && x1 < 40 && y0 >= 0 && y1 < 24)
	{
		char	col = 0;
		bool	hit = false;

		if (screen[WIDTH * y0 + x0] >= 128) col |= COL_00;
		if (screen[WIDTH * y0 + x1] >= 128) col |= COL_01;
		if (screen[WIDTH * y1 + x0] >= 128) col |= COL_10;
		if (screen[WIDTH * y1 + x1] >= 128) col |= COL_11;

		if (ball->vx < 0 && ((col & (COL_00 | COL_01)) == COL_00) || ((col & (COL_10 | COL_11)) == COL_10))
		{
			mirrorX = true;
			hit = true;
		}
		else if (ball->vx > 0 && ((col & (COL_00 | COL_01)) == COL_01) || ((col & (COL_10 | COL_11)) == COL_11))
		{
			mirrorX = true;
			hit = true;
		}

		if (ball->vy < 0 && ((col & (COL_00 | COL_10)) == COL_00) || ((col & (COL_01 | COL_11)) == COL_01))
		{
			mirrorY = true;
			hit = true;
		}
		else if (ball->vy > 0 && ((col & (COL_00 | COL_10)) == COL_10) || ((col & (COL_01 | COL_11)) == COL_11))
		{
			mirrorY = true;
			hit = true;
		}

		if (col)
		{
			brick_hit(x0, y0);
			brick_hit(x1, y0);
			brick_hit(x0, y1);
			brick_hit(x1, y1);
		}

		if (hit)
		{
			sound_trigger(BRICK_NOTE);
		}
	}

	if (mirrorY)
	{
		ball->vy = - ball->vy;
		ball->sy += ball->vy;
	}
	if (mirrorX)
	{
		ball->vx = - ball->vx;
		ball->sx += ball->vx;
	}
}

enum GameState
{
	GS_READY,			// Getting ready

	GS_BALL_LOCKED,		// The ball is locked on the paddle
	GS_PLAYING,			// Playing the game
	GS_BALL_DROPPED,	// The last ball has been dropped

	GS_GAME_OVER
};

struct Game
{
	GameState		state;
	Ball			balls[3];
	char			count;

};

static Game TheGame;

static void paddle_init(void)
{
	paddlevx = 0;
	paddlex = BALL_COORD(160, 0);


}

static void paddle_control(void)
{
	byte stick = joy_read(0);
	sbyte dx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);

	if (dx == 0)
	{
		if (paddlevx < 0)
			paddlevx = (paddlevx + 1) >> 1;
		else
			paddlevx >>= 1;
	}
	else
	{
		paddlevx += dx * 8;

		if (paddlevx >= 256)
			paddlevx = 256;
		else if (paddlevx < -256)
			paddlevx = -256;
	}

	paddlex += paddlevx;
	if (paddlex < BALL_COORD(-4, 0) || paddlex > BALL_COORD(320 - 48 + 4, 0))
	{
		paddlevx = -paddlevx;
		paddlex += paddlevx;
	}
}

static void paddle_move(void)
{
	gtia.hposp[0] = SCREEN_LEFT + (BALL_INT(paddlex) >> 1);
}

static void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	switch (state)
	{
	case GS_READY:
		brick_init();
		paddle_init();
		TheGame.count = 120;
		break;

	case GS_BALL_LOCKED:
		ball_init(TheGame.balls + 0, 0, paddlex + BALL_COORD(22, 0), BALL_COORD(184, 0), BALL_COORD(0,  0), BALL_COORD(0, 0));
		ball_init(TheGame.balls + 1, 1, paddlex + BALL_COORD(22, 0), BALL_COORD(184, 0), BALL_COORD(0,  0), BALL_COORD(0, 0));
		ball_init(TheGame.balls + 2, 2, paddlex + BALL_COORD(22, 0), BALL_COORD(184, 0), BALL_COORD(0,  0), BALL_COORD(0, 0));
		break;

	case GS_PLAYING:
		TheGame.balls[0].vy = BALL_COORD(-2, 32);
		TheGame.balls[0].vx = paddlevx >> 2;
		TheGame.balls[1].vy = BALL_COORD(-2, 16);
		TheGame.balls[1].vx = (paddlevx >> 2) - 16;
		TheGame.balls[2].vy = BALL_COORD(-2, 16);
		TheGame.balls[2].vx = (paddlevx >> 2) + 16;
		break;

	case GS_BALL_DROPPED:
		TheGame.count = 60;
		break;

	case GS_GAME_OVER:
		TheGame.count = 120;
		break;
	}
}

static void game_loop(void)
{
	switch (TheGame.state)
	{
	case GS_READY:
		paddle_control();
		if (!--TheGame.count)
			game_state(GS_BALL_LOCKED);
		break;
	case GS_BALL_LOCKED:
		paddle_control();
		TheGame.balls[0].sx = paddlex + BALL_COORD(22, 0);
		TheGame.balls[1].sx = paddlex + BALL_COORD(22, 0);
		TheGame.balls[2].sx = paddlex + BALL_COORD(22, 0);
		if (joy_fire(0))
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:
		paddle_control();
		for(char i=0; i<3; i++)
			ball_loop(TheGame.balls + i);
		for(char i=0; i<3; i++)
			ball_loop(TheGame.balls + i);
		if (!(TheGame.balls[0].active || TheGame.balls[1].active || TheGame.balls[2].active))
			game_state(GS_BALL_DROPPED);
		break;
	case GS_BALL_DROPPED:
		paddle_control();
		if (!--TheGame.count)
			game_state(GS_BALL_LOCKED);
		break;
	case GS_GAME_OVER:
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}
}


int main(void)
{
	const void * saved_display = (const void *)os_sdlst;
	void * saved_dli = os_vdslst;
	byte saved_dma = os_sdmctl, saved_chbase = os_chbas, saved_prior = os_gprior;
	byte saved_colors[5], saved_players[4];
	for (byte i = 0; i < 5; ++i)
		saved_colors[i] = os_color[i];
	for (byte i = 0; i < 4; ++i)
		saved_players[i] = os_pcolr[i];

	make_font();
	make_sprites();
	make_display();
	game_state(GS_READY);

	os_wait_frame();
	dli_off();
	os_sdlst = (word)display_list;
	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_WHITE;
	os_color[1] = COLOR_WHITE;
	os_color[2] = ATARI_COLOR(HUE_YELLOW, 0);
	os_color[3] = COLOR_BLACK;
	os_color[4] = COLOR_BLACK;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	os_gprior = PRIOR_P0123_PF0123;
	pmg_init(players, PMG_1LINE);
	volatile byte * paddle = pmg_player(0) + SCREEN_TOP + PADDLE_Y;
	for (byte i = 0; i < sizeof(paddle_shape); ++i)
		paddle[i] = paddle_shape[i];
	gtia.sizep[0] = 3;
	gtia.vdelay = 0;
	os_pcolr[0] = COLOR_GRAY3;
	for (byte i = 1; i < 4; ++i)
	{
		gtia.sizep[i] = 0;
		os_pcolr[i] = COLOR_WHITE;
	}
	sound_init();
	pokey.audctl = AUDCTL_CLOCK_15KHZ;
	os_wait_frame();
	dli_set(set_brick_color);

	while (!(cons_keys() & CONSOL_START))
	{
		brick_animate();
		game_loop();
		sound_loop();
		os_wait_frame();
		for (byte j = 0; j < 3; ++j)
			ball_move(TheGame.balls + j);
		paddle_move();
	}

	dli_off();
	os_vdslst = saved_dli;
	pmg_off();
	sound_off();
	pokey.audctl = 0;
	os_sdlst = (word)saved_display;
	os_sdmctl = saved_dma;
	os_chbas = saved_chbase;
	os_gprior = saved_prior;
	for (byte i = 0; i < 5; ++i)
		os_color[i] = saved_colors[i];
	for (byte i = 0; i < 4; ++i)
		os_pcolr[i] = saved_players[i];
	os_wait_frame();
	return 0;
}
