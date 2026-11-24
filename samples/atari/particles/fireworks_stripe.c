#include <atari/colors.h>
#include <atari/input.h>
#include <stdlib.h>
#include "../support/bitmap_demo.h"

// Single particle, with position, velocity and color pattern, using a next
// index for single linked list
struct Particle
{
	int				px, py, vx, vy;
	char			pat;
	char			next;
};

// Striped storage of particles, using an index for linkage
__striped	Particle	particles[256];

#pragma align(particles, 256)

// Index for used and free list heads
char	pfirst, pfree;

#define WIDTH 160
#define ROWS 100

static volatile byte * Hirows[ROWS];

static void pix_set(byte px, byte py, byte pat)
{
	Hirows[py][px >> 2] |= (0xc0 >> (2 * (px & 3))) & pat;
}

static void pix_clr(byte px, byte py)
{
	Hirows[py][px >> 2] &= ~(0xc0 >> (2 * (px & 3)));
}

// Init free list of particles
void particle_init(void)
{
	// Init address table for hires
	for (byte i = 0; i < ROWS; ++i)
		Hirows[i] = abm_row(&screen, i);

	// Init list heads, using index 0 for list termination
	pfirst = 0;
	pfree = 1;

	// Link all particles in free list
	for(int i=1; i<255; i++)
		particles[i].next = i + 1;
}

// Add a particle to the list
void particle_add(int px, int py, int vx, int vy, char pat)
{
	// Check if we have a particle left
	if (pfree)
	{
		// Use "auto" to generate a striped pointer
		char	i = pfree;
		auto	p = particles + pfree;

		// Remove from free list
		pfree = p->next;
		p->next = pfirst;

		// Add to used list
		pfirst = i;

		// Init particle data
		p->px = px;
		p->py = py;
		p->vx = vx;
		p->vy = vy;
		p->pat = pat;
	}
}

// Move particles in used list
void particle_move(void)
{
	// Start with first particle, remember previous
	// particle for list removal, using indices instead of pointers
	char	i = pfirst, pi = 0;

	// Zero is still list termination
	while (i)
	{
		// Use "auto" to generate a striped pointer
		auto	p = particles + i;

		// Clear previous particle image, using 9.7 fixed point
		pix_clr(p->px >> 7, p->py >> 7);

		// Advance position by velocity
		p->px += p->vx;
		p->py += p->vy;

		// Apply gravity
		p->vy += 8;

		// Check if particle is still on screen
		if (p->px < 0 || p->px >= 160 * 128 || p->py < 0 || p->py >= 100 * 128)
		{
			// Particle is offscreen, so we remove it from the used list

			// Remember next particle in used list
			char pn = p->next;

			// Remove from used list
			if (pi)
				particles[pi].next = pn;
			else
				pfirst = pn;

			// Attach to free list
			p->next = pfree;
			pfree = i;

			// Advance to next particle
			i = pn;
		}
		else
		{
			// Set image at new position
			pix_set(p->px >> 7, p->py >> 7, p->pat);

			// Advance to next particle
			pi = i;
			i = p->next;
		}
	}
}

// Normalized random function
int rnorm(void)
{
	int l0 = (rand() & 0xfff) - 0x800;
	int l1 = (rand() & 0xfff) - 0x800;
	int l2 = (rand() & 0xfff) - 0x800;
	int l3 = (rand() & 0xfff) - 0x800;

	return l0 + l1 + l2 + l3;
}

int main(void)
{
	BitmapDemoState saved;
	bitmap_demo_save(&saved);
	if (!show_bitmap(DL_MODED, ROWS))
		return 1;
	os_color[0] = COLOR_YELLOW;
	os_color[1] = ATARI_COLOR(2, 10);
	os_color[2] = COLOR_LIGHTBLUE;

	// Init particle system
	particle_init();

	char k = 0;
	for (word i = 0; i < 10000 && !(cons_keys() & CONSOL_START); ++i)
	{
		// Advance particles
		particle_move();

		if (k < 25)
		{
			// Add a particle from the left for the first third
			particle_add(4 * 64, 196 * 64, 256 + (rnorm() >> 6), -(384 + (rnorm() >> 6)), 0x55);
		}
		else if (k < 50)
		{
			// Add a particle from the right for the second third
			particle_add(316 * 64, 196 * 64, - (256 + (rnorm() >> 6)), -(384 + (rnorm() >> 6)), 0xaa);
		}
		else if (k < 75)
		{
			// Add a particle from the middle for the final third
			particle_add(160 * 64, 196 * 64, rnorm() >> 6, -(384 + (rnorm() >> 6)), 0xff);
		}

		// Advance thirds counter
		k++;
		if (k == 75)
			k = 0;
	}

	bitmap_demo_restore(&saved);
	return 0;

}
