#include <atari/colors.h>
#include <atari/input.h>
#include <atari/irq.h>
#include <atari/pmg.h>
#include <stdlib.h>
#include <string.h>
#define ATARI_SAMPLE_CUSTOM_REGION
#include "../support/bitmap_demo.h"

#pragma region(main, 0x2080, 0x8010, , , {code, data, bss, heap, stack})
#pragma stacksize(768)
#pragma heapsize(0)

static const byte MissileSprites[] = {
	#embed "../../resources/missilesprites.bin"
};
static const char MissileChars[1024] = {
	#embed "../../resources/missilechars.bin"
};

static byte hud[40];
#pragma align(hud, 64)
static byte font[1024];
#pragma align(font, 1024)
static byte dma[2048];
#pragma align(dma, 2048)
static byte cross[2][21];

static void put_pixel(int x, int y, byte color)
{
	abm_put(&screen, x >> 1, y, color);
}

static byte get_pixel(int x, int y)
{
	return abm_get(&screen, x >> 1, y);
}

static void line_pixels(int x0, int y0, int x1, int y1, byte color)
{
	abm_line(&screen, x0 >> 1, y0, x1 >> 1, y1, color);
}

static void circle_pixels(int x, int y, byte radius, byte color)
{
	abm_ellipse(&screen, x >> 1, y, (radius + 1) >> 1, radius, color);
}

static void draw_city(byte city, bool show)
{
	byte x0 = city < 3 ? 23 + 16 * city : 95 + 16 * (city - 3);
	const byte * src = MissileSprites + 64;
	for (byte y = 0; y < 20; ++y)
	{
		for (byte x = 0; x < 12; ++x)
			if (src[x >> 2] & (0xc0 >> (2 * (x & 3))))
				abm_put(&screen, x0 + x, 172 + y, show ? 2 : 0);
		src += 3;
	}
}

static void make_cross(void)
{
	for (byte y = 0; y < 21; ++y)
		for (byte x = 0; x < 12; ++x)
			if (MissileSprites[3 * y + (x >> 2)] & (0xc0 >> (2 * (x & 3))))
				cross[x >> 3][y] |= 0x80 >> (x & 7);
}

static void show_cross(int x, int y)
{
	PMGShape shape = {cross[0], 21, 43 + (x >> 1), 22 + y};
	pmg_move_player(0, &shape);
	shape.x += 8;
	shape.scanlines = cross[1];
	pmg_move_player(1, &shape);
}


__asm field_color
{
		pha
		lda $d40b
		sta $d40a
		cmp #107
		bcs ground
		cmp #101
		bcs sky
		lda #$2a
		sta $d017
		lda #$74
		jmp set_color
ground:
		lda #$0a
		sta $d017
		lda #$ee
		jmp set_color
sky:
		lda #$0a
		sta $d017
		lda #$74
set_color:
		sta $d018
		pla
		rti
}

static bool prepare_display(void)
{
	// The score line has its own ANTIC text mode. The rest is the bitmap.
	if (!abm_init(&screen, (volatile byte *)0x8010, 8000, 40, 200, DL_MODEE))
		return false;
	abm_clear(&screen, 0);
	volatile byte * end = dl_blank(display_list, 24);
	end = dl_lms(end, DL_MODE2 | DL_DLI, hud);
	for (byte row = 8; row < 200; ++row)
	{
		byte mode = DL_MODEE | (row == 171 || row == 183 ? DL_DLI : 0);
		if (row == 8 || row == 102)
			end = dl_lms(end, mode, abm_row(&screen, row));
		else
			*end++ = mode;
	}
	dl_jvb(end, display_list);
	return true;
}

// Joystick and crosshair control
volatile int		CrossX = 160, CrossY = 100;
bool	CrossP = false;
char	CrossDelay = 0;

// Structure for an explosion
struct Explosion
{
	int				x, y;	// Center of circle
	char			r;		// Radius of circle
	Explosion	*	next;	// Next explosion in list
};

// Structure for a missile (defensive and ICBM)
struct Missile
{
	int				sx, sy;	// start position
	int				tx, ty;	// target position
	int				x, y;	// current position
	int				dx, dy;	// distance in x and y
	int				d;		// error term for Bresenham
	sbyte			stepx;	// direction in x (+1 or -1)
	sbyte			cnt;	// speed counter
	Missile		*	next;	// next missile in list
};

// Storage space for explosion
Explosion		explosions[16];
// First free and first used explosion
Explosion	*	efree, * eused;

// Storage space for defending missiles
Missile			missiles[8];
// First free and first used missile
Missile		*	mfree, * mused;
// Number of missiles available
char			nmissiles;

// Storage space for ICBMs
Missile			icbms[16];
// First free and first used ICBM
Missile		*	ifree, * iused;
// Speed and number of ICBMs still incoming
char			icbmspeed, icbmcount;

// Cities not yet destroyed
bool 			cities[6];
char			ncities;

// Init status bar at top
void status_init(void)
{
	memset(hud, ' ', 40);

	for(char i=0; i<8; i++)
	{
		hud[i +  2] = '0';
		hud[i + 12] = '0';
	}
}

// Expand an 8x8 character to 16x16 on screen
void char_put(char cx, char cy, char c, char color)
{
	// Get pointer to glyph data
	const char	*	sp = MissileChars + 8 * c;

	// Loop over all pixel
	for(char y=0; y<8; y++)
	{
		char cl = sp[y];
		for(char x=0; x<8; x++)
		{
			// Draw two pixel if bit is set
			if (cl & 128)
			{
				put_pixel(cx + 2 * x, cy + 2 * y + 0, color);
				put_pixel(cx + 2 * x, cy + 2 * y + 1, color);
			}

			// Next bit
			cl <<= 1;
		}
	}
}

// Write a zero terminated string on screen
void char_write(char cx, char cy, const char * s, char color)
{
	// Loop over all characters
	while (*s)
	{
		char_put(cx, cy, *s, color);
		s++;
		cx += 16;
	}
}

// Increment the score from a given digit on
void score_inc(char digit, unsigned val)
{
	// Lowest digit to increment
	char	at = 9 - digit;

	// Loop while there is still score to account for
	while (val)
	{
		// Increment one character
		char	ch = hud[at] + val % 10;

		// Remove low digit from number
		val /= 10;

		// Check overflow
		if (ch > '9')
		{
			ch -= 10;
			val++;
		}

		hud[at] = ch;

		// Next higher character
		at --;
	}
}

// Reset score and update high score
void score_reset(void)
{
	// Find first digit, where score and highscore differ
	char i = 0;
	while (i < 8 && hud[i + 2] == hud[i + 12])
		i++;

	// Check if new score is higher
	if (i < 8 && hud[i + 2] > hud[i + 12])
	{
		// If so, copy to highscore
		while (i < 8)
		{
			hud[i + 12] = hud[i + 2];
			i++;
		}
	}

	// Clear score
	for(char i=0; i<8; i++)
		hud[i + 2] = '0';
}

// Update number of missiles
void status_missiles(char num)
{
	char	n = 0;

	// Draw full pairs
	while (2 * n + 1 < num)
	{
		hud[25 + n] = 92;
		n++;
	}
	// Draw remaining single one
	if (num & 1)
	{
		hud[25 + n] = 93;
		n++;
	}
	// Empty remainder
	while (n < 15)
	{
		hud[25 + n] = 94;
		n++;
	}
}

// Initialize explosion list
void explosion_init(void)
{
	// No explosion active
	eused = nullptr;

	// First free explosion element
	efree = explosions;
	// Build list
	for(char i=0; i<15; i++)
		explosions[i].next = explosions + i + 1;
	// Terminate last element
	explosions[15].next = nullptr;
}

// Start a new explosion
void explosion_start(int x, int y)
{
	// Free slot in list of explosions?
	if (efree)
	{
		// Move entry from free to used list
		Explosion	*	e = efree;
		efree = e->next;
		e->next = eused;
		eused = e;

		// Initialize position and size
		e->r = 0;
		e->x = x;
		e->y = y;
	}
}

// Animate all explosions
void explosion_animate(void)
{
	// Loop over active explosions with "e", use "ep" to point
	// to previous explosion, so we can remove the current explosion
	// from the list
	Explosion	*	e = eused, * ep = nullptr;
	while (e)
	{
		// Remember next entry in list
		Explosion	*	en = e->next;

		// Increment phase (radius)
		e->r++;

		// Advance every fourth frame
		if (!(e->r & 3))
		{
			// Draw or erase outer perimeter depending on growing or
			// shrinking explosion phase
			if (e->r <= 64)
				circle_pixels(e->x, e->y, e->r >> 2, 1);
			else
				circle_pixels(e->x, e->y, 33 - (e->r >> 2), 0);
		}

		// End of explosion live
		if (e->r == 128)
		{
			// Remove explosion from used list
			if (ep)
				ep->next = e->next;
			else
				eused = e->next;

			// Prepend it to free list
			e->next = efree;
			efree = e;
		}
		else
			ep = e;

		// Next explosion in list
		e = en;
	}
}

// Initialize defending missile list
void missile_init(void)
{
	mused = nullptr;
	mfree = missiles;
	for(char i=0; i<7; i++)
		missiles[i].next = missiles + i + 1;
	missiles[7].next = nullptr;
}

// Add a new defending missile
void missile_start(int sx, int sy, int tx, int ty)
{
	// Check if entry in free list and missile in silo remaining
	if (mfree && nmissiles > 0)
	{
		// Detach from free list
		Missile	*	m = mfree;
		mfree = m->next;

		// Attach to active list
		m->next = mused;
		mused = m;

		// Initialize start and target coordinates
		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;

		// Initialize line drawing parameters
		m->dy = m->sy - m->ty;
		m->dx = m->tx - m->sx;
		m->stepx = 1;
		if (m->dx < 0)
		{
			m->dx = -m->dx;
			m->stepx = -1;
		}

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;

		// Remove missile from silo
		nmissiles--;
		status_missiles(nmissiles);
	}
}

// Animate all active missiles
void missile_animate(void)
{
	Missile	*	m = mused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		// Advance missile position using one step of Bresenham
		if (m->d >= 0)
		{
			m->y--;
			m->d -= m->dx;
		}
		if (m->d < 0)
		{
			m->x += m->stepx;
			m->d += m->dy;
		}

		// Check if target reached
		if (m->y == m->ty)
		{
			// If so, clear line and start explosion
			line_pixels(m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
			explosion_start(m->x * 2, m->y);

			// Remove from active list
			if (mp)
				mp->next = m->next;
			else
				mused = m->next;
			m->next = mfree;
			mfree = m;
		}
		else
		{
			// Draw new pixel in missile trace
			put_pixel(m->x * 2, m->y, 3);
			mp = m;
		}

		m = mn;
	}
}

// Initialize incoming ICBM list
void icbm_init(void)
{
	iused = nullptr;
	ifree = icbms;
	for(char i=0; i<15; i++)
		icbms[i].next = icbms + i + 1;
	icbms[15].next = nullptr;
}

// Add a new ICBM to the attacking set
void icbm_start(int sx, int sy, int tx, int ty)
{
	if (icbmcount && ifree)
	{
		Missile	*	m = ifree;
		ifree = m->next;
		m->next = iused;
		iused = m;

		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;
		m->cnt = 0;

		m->dy = m->ty - m->sy;
		m->dx = m->tx - m->sx;
		m->stepx = 1;
		if (m->dx < 0)
		{
			m->dx = -m->dx;
			m->stepx = -1;
		}

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;

		icbmcount--;
	}
}

void icbm_animate(void)
{
	Missile	*	m = iused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		// Check speed of ICBMs
		m->cnt += icbmspeed;
		while (m->cnt > 0)
		{
			m->cnt -= 32;

			// Draw pixel in trace
			put_pixel(m->x * 2, m->y, 2);

			// Advance using Bresenham
			if (m->d >= 0)
			{
				m->y++;
				m->d -= m->dx;
			}
			if (m->d < 0)
			{
				m->x += m->stepx;
				m->d += m->dy;
			}

			// Check if colliding with cloud or target reached
			if (get_pixel(m->x * 2, m->y) == 1 || m->y == m->ty)
			{
				// If so, clear trace and start explosion
				line_pixels(m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
				explosion_start(m->x * 2, m->y);

				// Check if we hit the ground
				if (m->y == m->ty)
				{
					// If so, find matching city
					int	x = m->x * 2;
					char	ix;
					if (x > 160)
						ix = ((x - 202) >> 5) + 3;
					else
						ix = (x - 58) >> 5;

					// Destroy, destroy, annihilate, kill, kill
					if (ix < 6 && cities[ix])
					{
						ncities--;
						cities[ix] = false;
						draw_city(ix, false);
					}
				}

				// Add score for destroyed ICBM
				score_inc(0, 25);

				// Remove from list
				if (mp)
					mp->next = m->next;
				else
					iused = m->next;
				m->next = ifree;
				ifree = m;
				m = nullptr;

				// End loop early
				break;
			}
		}

		// If ICBM still in flight
		if (m)
		{
			// Remember for list management
			mp = m;
			// Draw white head
			put_pixel(m->x * 2, m->y, 1);
		}

		m = mn;
	}
}

// Initialize game screen
static void screen_init(void)
{
	abm_fill_rect(&screen, 0, 8, 160, 184, 0);
	abm_fill_rect(&screen, 0, 192, 160, 8, 3);
	const ABitmapPoint pads[3][4] = {
		{{0,192},{8,184},{16,184},{24,192}},
		{{68,192},{76,184},{88,184},{96,192}},
		{{136,192},{144,184},{152,184},{159,192}}
	};
	for (byte i = 0; i < 3; ++i)
		abm_fill_polygon(&screen, pads[i], 4, 3);
	for (byte i = 0; i < 6; ++i)
		if (cities[i])
			draw_city(i, true);
}

static void screen_clear(void)
{
	abm_fill_rect(&screen, 0, 8, 160, 176, 0);
	for (byte i = 0; i < 6; ++i)
		if (cities[i])
			draw_city(i, true);
}

// Interrupt routine for joystick control, called by raster IRQ at bottom
// of screen

static void cross_control(void)
{
	// Poll joystick
	byte stick = joy_read(0);
	sbyte dx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
	sbyte dy = ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);

	// Move crosshair coordinates
	int cx = CrossX + 2 * dx, cy = CrossY + 2 * dy;

	// Stop at edges of screen
	if (cx < 8)
		cx = 8;
	else if (cx > 312)
		cx = 312;
	if (cy < 20)
		cy = 20;
	else if (cy > 172)
		cy = 172;

	// Move crosshair sprite
	show_cross(cx, cy);
	CrossX = cx;
	CrossY = cy;

	// Check button
	if (joy_fire(0))
	{
		// Avoid quickfire and bouncing
		if (CrossDelay == 0)
		{
			// Request fire from non interrupt code
			CrossP = true;
			CrossDelay = 4;
		}
	}
	else if (CrossDelay > 0)
		CrossDelay--;
}

enum GameState
{
	GS_READY,		// Getting ready
	GS_LEVEL,		// Show level message
	GS_PLAYING,		// Playing the game
	GS_BONUS,		// Landed on pad
	GS_ARMAGEDDON, 	// Show end game animation
	GS_END			// Wait for restart
};

// State of the game
struct Game
{
	GameState	state;
	byte		count, level;

}	TheGame;	// Only one game, so global variable

// Character buffer for some texts
char	cbuffer[10];

void game_state(GameState state)
{

	TheGame.state = state;

	switch(state)
	{
	case GS_READY:
		// Start of new game
		score_reset();
		screen_init();
		char_write(40, 100, s"READY PLAYER 1", 3);

		// New cities
		for(char i=0; i<6; i++)
			cities[i] = true;
		ncities = 6;
		TheGame.count = 60;

		// Starting at level 1
		TheGame.level = 0;
		break;

	case GS_LEVEL:
		// Advance to next level
		TheGame.level++;
		utoa(TheGame.level ,cbuffer, 10);
		screen_clear();
		char_write(96, 100, s"LEVEL", 3);
		char_write(96 + 16 * 6, 100, cbuffer, 3);
		TheGame.count = 30;
		break;

	case GS_PLAYING:
		// Avoid old fire request
		CrossP = false;

		// Setup display
		screen_init();
		missile_init();
		explosion_init();
		icbm_init();

		// A new set of 30 missiles
		nmissiles = 30;
		status_missiles(nmissiles);

		// Game parameters based on level
		if (TheGame.level < 112)
			icbmspeed = 8 + TheGame.level;
		else
			icbmspeed = 120;

		if (TheGame.level < 50)
			icbmcount = 5 + TheGame.level / 2;
		else
			icbmcount = 30;

		TheGame.count = 15;
		break;

	case GS_BONUS:
	{
		// Show bonus

		unsigned bonus = ncities * 50 + nmissiles * 5;
		utoa(bonus, cbuffer, 10);

		char_write(96, 100, s"BONUS", 3);
		char_write(96 + 16 * 6, 100, cbuffer, 3);

		score_inc(0, bonus);
		TheGame.count = 30;
	}	break;

	case GS_ARMAGEDDON:
		TheGame.count = 0;
		break;

	case GS_END:
		char_write(104, 92, s"THE END", 0);
		TheGame.count = 120;
		break;
	}
}

// Main game play code
void game_play(void)
{
	// Check if fire request
	if (CrossP)
	{
		int cx = CrossX, cy = CrossY;

		// Find launch site
		int	sx = 160;
		if (cx < 120)
			sx = 24;
		else if (cx > 200)
			sx = 296;

		// Fire missile
		missile_start(sx, 184, cx, cy);

		// Reset request
		CrossP = false;
	}

	// Wait for next ICBM to enter the game
	if (!--TheGame.count)
	{
		// Pick target city
		char ci;
		do {
			ci = rand() & 7;
		} while (ci >= 6 || !cities[ci]);

		int	cx;
		if (ci < 3)
			cx = 58 + 32 * ci;
		else
			cx = 202 + 32 * (ci - 3);

		// Launch ICBM
		icbm_start((rand() & 0xff) + 32, 0, cx, 184);

		// Next launch time
		TheGame.count = 8 + (rand() & 63);
	}

	// Advance defending missiles by four pixels
	for(char i=0; i<4; i++)
		missile_animate();

	// Advance ICBMs
	icbm_animate();

	// Show explosions
	explosion_animate();
}

// Main game loop, entered every VSYNC, slower if too busy with explosions
void game_loop(void)
{
	switch(TheGame.state)
	{
	case GS_READY:
		if (!--TheGame.count)
			game_state(GS_LEVEL);
		break;
	case GS_LEVEL:
		if (!--TheGame.count)
			game_state(GS_PLAYING);
		break;

	case GS_PLAYING:
		game_play();

		// Check for level and game end conditions
		if (!icbmcount && !iused && !eused)
			game_state(GS_BONUS);
		else if (ncities == 0)
			game_state(GS_ARMAGEDDON);
		break;

	case GS_BONUS:
		if (!--TheGame.count)
			game_state(GS_LEVEL);
		break;

	case GS_ARMAGEDDON:
		// Draw end game animation
		TheGame.count++;
		circle_pixels(160, 100, TheGame.count, 1);
		explosion_animate();
		if (TheGame.count == 90)
			game_state(GS_END);
		break;

	case GS_END:
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte chbase = os_chbas, player0 = os_pcolr[0], player1 = os_pcolr[1];
	void * saved_dli = os_vdslst;
	if (!prepare_display())
		return 1;
	memcpy(font, MissileChars, sizeof(font));
	make_cross();
	status_init();
	os_chbas = (word)font >> 8;
	os_color[0] = COLOR_WHITE;
	os_color[1] = COLOR_WHITE;
	os_color[2] = COLOR_BLACK;
	os_color[4] = COLOR_BLACK;
	os_gprior = PRIOR_P0123_PF0123;
	os_sdmctl = DMACTL_DLIST | DMACTL_PF_NORMAL;
	pmg_init(dma, PMG_1LINE);
	gtia.sizep[0] = gtia.sizep[1] = gtia.vdelay = 0;
	os_pcolr[0] = os_pcolr[1] = COLOR_WHITE;
	game_state(GS_READY);
	dl_set(display_list);
	os_wait_frame();
	dli_set(field_color);

	while (!(cons_keys() & CONSOL_START))
	{
		cross_control();
		game_loop();
		os_wait_frame();
	}

	dli_off();
	os_vdslst = saved_dli;
	pmg_off();
	os_chbas = chbase;
	os_pcolr[0] = player0;
	os_pcolr[1] = player1;
	bitmap_demo_restore(&saved);
	return 0;
}
