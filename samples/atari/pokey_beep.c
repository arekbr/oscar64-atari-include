#include <atari/os.h>
#include <atari/sound.h>

int main(void)
{
	// Use whole video frames for a one-second delay and note.
	byte frames_per_second = os_palnts ? 50 : 60;

	sound_init();
	os_wait_frames(frames_per_second);
	sound(0, 63, SOUND_PURE, 8);
	os_wait_frames(frames_per_second);
	sound_off();

	for (;;)
		os_wait_frame();
	return 0;
}
