#ifndef ATARI_SAMPLE_DISPLAY_STATE_H
#define ATARI_SAMPLE_DISPLAY_STATE_H

#include <atari/os.h>

struct BitmapDemoState
{
	word list;
	byte dma, prior, colors[5];
};

static void bitmap_demo_save(BitmapDemoState * state)
{
	state->list = os_sdlst;
	state->dma = os_sdmctl;
	state->prior = os_gprior;
	for (byte i = 0; i < 5; ++i)
		state->colors[i] = os_color[i];
}

static void bitmap_demo_restore(const BitmapDemoState * state)
{
	os_wait_frame();
	os_sdlst = state->list;
	os_sdmctl = state->dma;
	os_gprior = state->prior;
	for (byte i = 0; i < 5; ++i)
		os_color[i] = state->colors[i];
	os_wait_frame();
}

#endif
