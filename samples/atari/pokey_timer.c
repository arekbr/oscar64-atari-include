#include <atari/console.h>
#include <atari/os.h>
#include <atari/pokey.h>
#include <atari/ptimer.h>

static volatile word ticks;

static __interrupt void count_tick(void)
{
	ticks++;
}

int main(void)
{
	con_clrscr();

	// Use the 64 kHz clock and mute the audio channel used by timer 1.
	ptimer_disable(PTIMER_1);
	pokey.audctl = 0;
	pokey.aud[0].audc = 0;
	ptimer_attach(PTIMER_1, count_tick);
	ptimer_period(PTIMER_1, 255);
	ptimer_restart_all();
	ptimer_enable(PTIMER_1);

	os_wait_frames(30);

	// Detaching stops the interrupts before the two-byte counter is read.
	ptimer_detach(PTIMER_1);
	con_printf("Timer IRQs in 30 frames: %u\n", (unsigned)ticks);
	return 0;
}
