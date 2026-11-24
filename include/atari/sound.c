#include "sound.h"

void sound_init(void)
{
	pokey.audctl = 0;
	pokey.skctl = 3;
	sound_off();
}

void sound(byte ch, byte freq, byte dist, byte vol)
{
	pokey.aud[ch & 3].audf = freq;
	pokey.aud[ch & 3].audc = (dist << 4) | (vol & 15);
}

void sound_off(void)
{
	for (byte i = 0; i < 4; i++)
		pokey.aud[i].audc = 0;
}
