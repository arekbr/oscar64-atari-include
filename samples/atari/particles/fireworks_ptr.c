#include <atari/colors.h>
#include <atari/input.h>
#include <stdlib.h>
#include "../support/bitmap_demo.h"

// Single particle, with position, velocity and color pattern, using a next
// pointer for single linked list
struct Particle
{
	int				px, py, vx, vy;
	char			pat;
	Particle	*	next;
};

// Storage for up to 256 particles
Particle	particles[256];

// Heads of used and free list
Particle	*	pfirst, * pfree;

// Lookup table for hires row buffer
#define WIDTH 160
#define ROWS 100

static volatile byte * Hirows[ROWS];

static void pix_set(unsigned px, unsigned py, byte pat)
{
	Hirows[(py >> 1)][(px >> 1) >> 2] |= (0xc0 >> (2 * ((px >> 1) & 3))) & pat;
}

static void pix_clr(unsigned px, unsigned py)
{
	Hirows[(py >> 1)][(px >> 1) >> 2] &= ~(0xc0 >> (2 * ((px >> 1) & 3)));
}

// Init free list of particles
void particle_init(void)
{
	// Init address table for hires
	for (byte i = 0; i < ROWS; ++i)
		Hirows[i] = abm_row(&screen, i);

	// Init list heads
	pfirst = nullptr;
	pfree = particles;

	// Link all particles in free list
	for(int i=0; i<255; i++)
		particles[i].next = particles + i + 1;
}

// Add a particle to the list
void particle_add(int px, int py, int vx, int vy, char pat)
{
	// Check if we have a particle left
	if (pfree)
	{
		// Remove from free list
		Particle	*	p = pfree;
		pfree = pfree->next;

		// Add to used list
		p->next = pfirst;
		pfirst = p;

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
	// particle for list removal
	Particle	*	p = pfirst, * pp = nullptr;

	// Loop over all particles in used list
	while (p)
	{
		// Clear previous particle image, using 10.6 fixed point
		pix_clr(p->px >> 6, p->py >> 6);

		// Advance position by velocity
		p->px += p->vx;
		p->py += p->vy;

		// Apply gravity
		p->vy += 8;

		// Check if particle is still on screen
		if (p->px < 0 || p->px >= 320 * 64 || p->py < 0 || p->py >= 200 * 64)
		{
			// Particle is offscreen, so we remove it from the used list

			// Remember next particle in used list
			Particle	*	pn = p->next;

			// Remove from used list
			if (pp)
				pp->next = pn;
			else
				pfirst = pn;

			// Attach to free list
			p->next = pfree;
			pfree = p;

			// Advance to next particle
			p = pn;
		}
		else
		{
			// Set image at new position
			pix_set(p->px >> 6, p->py >> 6, p->pat);

			// Advance to next particle
			pp = p;
			p = p->next;
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
