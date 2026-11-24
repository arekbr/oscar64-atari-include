#include <atari/os.h>
#include <atari/senv.h>

// Attack, decay and release are measured in video frames.
static const struct SEnv note = {25, 20, 10, 45};

int main(void)
{
	sound_init();
	senv_init();

	os_wait_frames(50);
	senv_on(0, snd_note[21], SOUND_PURE, 15, &note);
	os_wait_frames(90);
	senv_off(0);

	// Let the release finish before removing the vertical blank handler.
	os_wait_frames(55);
	senv_shutdown();
	for (;;)
		os_wait_frame();
	return 0;
}
