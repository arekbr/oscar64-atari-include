#include <atari/input.h>
#include <atari/sound.h>
#include <stdlib.h>
#include "shmup_video.h"

#pragma stacksize(512)
#pragma heapsize(0)

static byte sound_ticks[2];

static void play_sound(byte channel, byte pitch, byte wave, byte frames)
{
	sound(channel, pitch, wave, 9);
	sound_ticks[channel] = frames;
}

static void sound_frame(void)
{
	for (byte channel = 0; channel < 2; ++channel)
		if (sound_ticks[channel] && --sound_ticks[channel] == 0)
			pokey.aud[channel].audc = 0;
}

// List of enemies
struct Enemy
{
	int		px;		// absolute horizontal position
	byte	py;		// absolute vertical position
	sbyte	dx;		// horizontal speed
	byte	state;	// state of the enemy
	byte	pad0, pad1, pad2;	// padding to align at 8 bytes
}	enemies[5];

// Shot data
struct Shot
{
	byte		ty, x, ry, n;
	sbyte		dx;		// Direction of shot
	Shot	*	next;	// Next shot in list
}	shots[18];		// Pre-allocated shots

Shot	*	freeShot;

// Player data
struct Player
{
	int		spx;	// absolute screen position
	sbyte	vpx;	// horizontal velocity
	sbyte	ax;		// acceleration
	char	aphase;	// animation phase
	char	spy;	// vertical position
	char	fdelay;	// auto fire delay
}	player;

// Game state
enum GameState
{
	GS_START,			// Waiting for the game to start

	GS_READY,			// Getting ready

	GS_PLAYING,			// Actually playing
	GS_EXPLODING,		// Player ship is exploding

	GS_GAME_OVER		// Game is over
};

// Game data
struct Game
{
	GameState	state;	// Current game state
	char		count;	// Countdown for states that change after delay
	char		edelay;	// enemy wave delay
	char		ecount;	// number of live enemies
	char		escore;	// score not yet accumulated
	char		lives;	// number of player lives left
}	game;


//                          0123456789012345678901234567890123456789
const char StatusText[] = s" Score: 000000  High: 000000   Lives: 0 ";

// initialize status line
void status_init(void)
{
	for(char i=0; i<40; i++)
		hud[i] = StatusText[i];
}

// update the status line
void status_update(void)
{
	// Set number of lives
	hud[38] = game.lives + '0';

	// Increment the score from a given digit on
	if (game.escore)
	{
		char	at, val = 1;

		// Check for 100s, 10s and 1s and set digit to increment
		// appropriately
		if (game.escore >= 100)
		{
			game.escore -= 100;
			at = 11;
		}
		else if (game.escore >= 10)
		{
			game.escore -= 10;
			at = 12;
		}
		else
		{
			val = game.escore;
			game.escore = 0;
			at = 13;
		}

		// Loop while there is still score to account for
		while (val)
		{
			// Increment one character
			char	ch = hud[at] + val;
			val = 0;

			// Check overflow
			if (ch > '9')
			{
				ch -= 10;
				val = 1;
			}

			hud[at] = ch;

			// Next higher character
			at --;
		}
	}
}

// Reset score and update high score
void score_reset(void)
{
	// Find first digit, where score and highscore differ
	char i = 0;
	while (i < 6 && hud[i + 8] == hud[i + 22])
		i++;

	// Check if new score is higher
	if (i < 6 && hud[i + 8] > hud[i + 22])
	{
		// If so, copy to highscore
		while (i < 6)
		{
			hud[i + 22] = hud[i + 8];
			i++;
		}
	}

	// Clear score
	for(char i=0; i<6; i++)
		hud[i + 8] = '0';
}

// Initialize the list of shots
void shot_init(void)
{
	// First shot is just a list head and doubles
	// as the last shot in the list as well, so a
	// circular list

	shots[0].next = shots;
	shots[0].ty = 6;

	// Build list of free shots
	freeShot = shots + 1;
	for(char i=1; i<17; i++)
		shots[i].next = shots + i + 1;
	shots[17].next = nullptr;
}

// Add a shot to the active list
void shot_add(int sx, char sy, sbyte dx)
{
	// Actual screen position
	char	py = sy - 14;
	char	gy = py >> 5;
	char	ey = (py >> 3) & 3;
	char	ry = py & 7;

	// Get a new shot structure from the free list
	Shot * s = freeShot;
	if (!s)
		return;
	freeShot = s->next;

	// Insert it into the sorted list of active shots
	Shot	*	p = shots;
	while (p->next->ty < gy)
		p = p->next;
	s->next = p->next;
	p->next = s;

	// Init structure
	s->ty = gy;
	s->ry = ry;
	s->dx = dx;

	// Horizontal start position based on player sprite
	if (dx < 0)
	{
		char	x = (sx) >> 3;
		s->n = x - 1;
		s->x = 40 * ey + x;
	}
	else
	{
		char	x = (sx + 24) >> 3;
		s->x = 40 * ey + x;
		s->n = 39 - x;
	}
}

static byte stars[20], xcollision[256];

static void tiles_draw(word x)
{
	draw_world(x);
	byte phase = ((x >> 3) - 4) & 3;
	byte first = 4 + phase;
	for (byte row = 0; row < ROWS; ++row)
	{
		byte * spot = playfield[screen_page][row] + first + stars[row];
		if (*spot == blank_ch[row >> 1])
			*spot = STAR;
	}

	Shot * prev = shots;
	byte glyph = screen_page * SHOT_GLYPHS;
	byte glyph_end = glyph + SHOT_GLYPHS;
	while (prev->next != shots)
	{
		Shot * shot = prev->next;
		shot->x += shot->dx;
		--shot->n;
		byte y = 4 * shot->ty + shot->x / 40;
		byte x = first + shot->x % 40;
		if (y < ROWS && glyph < glyph_end)
		{
			byte * ch = &playfield[screen_page][y][x];
			byte * font = fonts[y >> 1];
			memcpy(font + 8 * glyph, font + 8 * *ch, 8);
			font[8 * glyph + shot->ry] = 0xff;
			*ch = glyph++;
		}
		if (shot->n)
			prev = shot;
		else
		{
			prev->next = shot->next;
			shot->next = freeShot;
			freeShot = shot;
		}
	}
}

// Initialize player
void player_init(void)
{
	// Set up start position and direction
	player.vpx = 0;
	player.ax = 1;
	player.spy = 100;
	player.fdelay = 0;

	// Show sprites
	spr_set(0, true, 160, 100, 64,          6, true, false, false);
	spr_set(7, true, 160, 100, 64 + 16, 12, true, false, false);
}

// Use the joystick to control the player
void player_control(void)
{
	// Poll the joystick
	byte stick = joy_read(0);
	sbyte jx = ((stick & JOY_RIGHT) != 0) - ((stick & JOY_LEFT) != 0);
	sbyte jy = ((stick & JOY_DOWN) != 0) - ((stick & JOY_UP) != 0);

	// Change horizontal direction if joystick left or right
	if (jx != 0)
		player.ax = jx;

	// Change vertical position if joystick up or down
	player.spy += 2 * jy;
	if (player.spy < 14)
		player.spy = 14;
	else if (player.spy > 14 + 159)
		player.spy = 14 + 159;

	// Animate/accelerate based on requested direction
	if (player.ax > 0)
	{
		// Speed up until max speed
		if (player.vpx < 32)
			player.vpx++;

		// Adapt animation phase for direction change
		if (player.aphase > 0 && player.aphase < 16)
			player.aphase--;
		else if (player.aphase >= 16 && player.aphase < 32)
			player.aphase++;
	}
	else if (player.ax < 0)
	{
		if (player.vpx > - 32)
			player.vpx--;

		if (player.aphase >= 0 && player.aphase < 16)
			player.aphase++;
		else if (player.aphase > 16 && player.aphase < 32)
			player.aphase--;

	}

	// Modulus animation phase
	player.aphase &= 31;

	// Set player sprite image based on animation phase
	spr_image(0, 64 + (player.aphase >> 1));
	spr_image(7, 80 + (player.aphase >> 1));

	// Player position based on speed
	int	px = 148 - 4 * player.vpx;

	// Check for player shooting
	if (player.fdelay)
		player.fdelay--;
	else if (joy_fire(0))
	{
		if (player.aphase < 4 || player.aphase >= 29)
		{
			// Fire right
			shot_add(px, player.spy, 1);
			play_sound(0, 34, SOUND_PURE, 6);
		}
		else if (player.aphase >= 13 && player.aphase < 20)
		{
			// Fire left
			shot_add(px, player.spy, -1);
			play_sound(0, 34, SOUND_PURE, 6);
		}

		// Delay next shot by six frames
		player.fdelay = 6;
	}

	// Move player sprite
	spr_move(0, 24 + px, 50 + player.spy);
	spr_move(7, 32 + px, 58 + player.spy);
}

// Advance absolute player/screen position
void player_move(void)
{
	player.spx += player.vpx >> 2;
}

// Initialize game structure
void game_init(void)
{
	game.edelay = 80;
	game.ecount = 0;
	game.escore = 0;
	game.lives = 4;
	player.spx = 40;
}

// Move all active enemies
void enemies_move(void)
{
	// Loop over potential five enemies
	for(char i=0; i<5; i++)
	{
		// Check if enemy is active
		if (enemies[i].state)
		{
			// Advance position
			enemies[i].px += enemies[i].dx;

			// Check for relative position to player
			int	rx = enemies[i].px - player.spx;

			if (rx < -192 || rx >= 480)
			{
				// Enemy is outside scope, remove it from active set
				enemies[i].state = 0;
				game.ecount--;
				spr_show(2 + i, false);
			}
			else
			{
				// Move enemy sprite
				spr_move(2 + i, rx + 24, enemies[i].py + 50);

				if (enemies[i].state & 0x80)
				{

				}
				else
				{
					// Enemy is exploding
					spr_color(2 + i, 7);
					spr_image(2 + i, 127 - (enemies[i].state >> 2));
					enemies[i].state--;
					if (enemies[i].state == 0)
					{
						// Explosion complete, remove from active set
						spr_show(2 + i, false);
						game.ecount--;
					}
				}
			}
		}
	}
}

// Spawn one new enemy outside screen
void enemies_spawn(void)
{
	// Seed for enemy
	char	u = rand();

	// Index of enemy
	char	e = game.ecount;

	// We know there are only five, help the compiler
	// with the indexing
	__assume(e < 5);

	// Speed of enemy
	sbyte	v = 1 + (u & 3);

	// Vertical position
	enemies[e].py = 29 + 32 * e;

	// Speed and direction
	enemies[e].dx = player.vpx < 0 ? v : -v;

	// Position left or right outside
	enemies[e].px = (player.vpx < 0 ? player.spx - 56 : player.spx + 320) + ((u >> 1) & 31);

	// Initial status is alive
	enemies[e].state = 0x80;

	// Set enemy sprite outside screen
	int	rx = enemies[e].px - player.spx;
	spr_set(2 + e, true, rx + 24, enemies[e].py + 50, player.vpx < 0 ? 97 : 96, 14, true, false, false);

	// One more enemy ready
	game.ecount++;
}

// Remove and reset all enemies
void enemies_reset(void)
{
	for(char i=0; i<5; i++)
	{
		// Hide sprite and clearout state
		spr_show(2 + i, false);
		enemies[i].state = 0x00;
	}

	// Delay for next enemy to show up
	game.edelay = 80;
	game.ecount = 0;
}

// Check collision player with enemy
bool player_check(void)
{
	// Index of enemy in players tile row
	char	e = (player.spy - 14) >> 5;

	// Is it active?
	if (e < 5 && (enemies[e].state & 0x80))
	{
		// Relative horizontal position
		int	rx = enemies[e].px - player.spx;

		rx -= 148 - 4 * player.vpx;

		// Too close, we are toast
		if (rx >= - 12 && rx <= 12)
		{
			// Enemy explodes as well
			enemies[e].state = 64;
			return true;
		}
	}

	return false;
}

// Check collision shot with enemies
bool shots_check(void)
{
	// First shot in list (actually the head, or the shot
	// before the current one)
	Shot	*	ps = shots;
	bool		hit = false;

	// Loop over all five enemies
	for(char i=0; i<5; i++)
	{
		// Only check live and active enemies
		if (enemies[i].state & 0x80)
		{
			// Enemy position on screen
			sbyte	rx = (enemies[i].px - player.spx) >> 3;

			// Check for actually on screen
			if (rx >= 0 && rx < 40)
			{
				// Ignore all shots in tile rows before the
				// current row
				Shot	*	ss = ps->next;
				while (ss->ty < i)
				{
					ps = ss;
					ss = ps->next;
				}

				// Now loop over all shots in the current tile row
				while (ss->ty == i)
				{
					// Check horizontal collision
					if (xcollision[(char)(ss->x - rx)])
					{
						// Remove shot from active list and put it into free list
						ps->next = ss->next;
						ss->next = freeShot;
						freeShot = ss;

						// Mark enemy as exploding
						enemies[i].state = 64;

						// Increment score
						game.escore++;
						hit = true;

						// Done checking this row
						break;
					}

					ps = ss;
					ss = ps->next;
				}
			}
		}
	}

	return hit;
}

// Advance game state
void game_state(GameState state)
{
	// Set new state
	game.state = state;

	switch (state)
	{
	case GS_START:
		game_init();
		game_state(GS_READY);
		break;

	case GS_READY:
		game.count = 120;
		spr_show(0, false);
		spr_show(7, false);

		break;
	case GS_PLAYING:
		player_init();
		break;
	case GS_EXPLODING:
		play_sound(0, 52, SOUND_POLY17, 48);
		spr_color(0, 7);
		spr_color(7, 8);
		game.count = 64;
		break;
	case GS_GAME_OVER:
		score_reset();
		game.count = 120;
		break;
	}
}

// Work for current frame
void game_loop(void)
{
	// Draw the background
	tiles_draw(player.spx & 4095);

	switch (game.state)
	{
	case GS_START:
		break;
	case GS_READY:
		// Scroll to start position
		player.spx -= game.count >> 2;

		if (!--game.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:
		// Control player
		player_control();

		// Move player
		player_move();

		// Check for new enemies
		if (game.edelay)
		{
			game.edelay--;
			if (game.edelay < 10 && !(game.edelay & 1))
				enemies_spawn();
		}
		else
		{
			// Move enemies
			enemies_move();

			// All enemies gone, update delay for next squad
			if (!game.ecount)
				game.edelay = 64 + (rand() & 63);
		}

		// Check shots
		if (shots_check())
			play_sound(1, 38, SOUND_POLY17, 28);

		// Check player enemy collision
		if (player_check())
			game_state(GS_EXPLODING);
		break;

	case GS_EXPLODING:
		// Animate player
		spr_image(0, 127 - (game.count >> 2));
		spr_image(7, 127 - (game.count >> 2));

		player_move();
		enemies_move();

		// Wait for explosion to finish
		if (!--game.count)
		{
			enemies_reset();
			game.lives--;

			// More lives?
			if (game.lives)
				game_state(GS_READY);
			else
				game_state(GS_GAME_OVER);
		}
		break;
	case GS_GAME_OVER:
		if (!--game.count)
			game_state(GS_START);
		break;
	}

	// Update sound effects
	sound_frame();

	// Update status line
	status_update();
}


int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	byte old_chbas = os_chbas, old_players[4];
	for (byte i = 0; i < 4; ++i) old_players[i] = os_pcolr[i];
	void * old_dli = os_vdslst;
	show_video();
	sound_init();
	status_init();
	for (byte i = 0; i < ROWS; ++i) stars[i] = rand() % 40;
	for (byte i = 0; i < 160; i += 40)
		for (byte j = 0; j < 3; ++j) xcollision[i + j] = 1;
	shot_init();
	game_init();
	player_init();
	game_state(GS_READY);
	game_loop();
	present_video();
	os_wait_frame();
	dli_set(playfield_dli);

	while (!(cons_keys() & CONSOL_START))
	{
		game_loop();
		present_video();
	}

	dli_off();
	os_vdslst = old_dli;
	pmg_off();
	sound_off();
	os_chbas = old_chbas;
	for (byte i = 0; i < 4; ++i) os_pcolr[i] = old_players[i];
	bitmap_demo_restore(&saved);
	return 0;
}
