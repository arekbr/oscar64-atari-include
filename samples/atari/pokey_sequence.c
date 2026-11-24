#include <atari/os.h>
#include <atari/pseq.h>

// PAL dividers for the joined lead voice and the 15 kHz bass voice.
static const struct PSQNote notes[] = {
	{2008, 41, 0},
	{1503, 41, 0}
};
static const struct PSQInstrument instruments[] = {
	{12, 12, 0, 0, 0, 5},
	{7, 7, 0, 0, 0, 5}
};
static const struct PSQEvent lead[] = {{1, 0, 35, 0}, {2, 0, 35, 0}};
static const struct PSQEvent bass[] = {{1, 1, 70, 0}};
static const struct PSQSong songs[] = {
	{{{lead, 2, PSQ_NO_LOOP}, {bass, 1, PSQ_NO_LOOP}, {0, 0, PSQ_NO_LOOP}}}
};
static const struct PSQBank bank = {notes, instruments, songs, 2, 2, 1};

int main(void)
{
	if (psq_init(&bank) != PSQ_OK)
		return 1;
	os_wait_frames(50);
	if (psq_play(0) != PSQ_OK)
		return 1;

	// The main loop is the sole tick consumer. No interrupt hook is needed.
	while (psq_playing())
	{
		os_wait_frame();
		psq_tick();
	}

	for (;;)
		os_wait_frame();
	return 0;
}
