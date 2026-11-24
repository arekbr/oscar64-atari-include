#include "senv.h"
#include "pokey.h"
#include "os.h"

#define OS_SETVBV		0xe45c
#define SETVBV_IMMEDIATE	6

#define CMD_ON	1
#define CMD_OFF	2

const byte snd_note[36] = {
	241, 228, 215, 203, 191, 180, 170, 161, 152, 143, 135, 127,		// C3..B3
	120, 113, 107, 101,  95,  90,  85,  80,  75,  71,  67,  63,		// C4..B4 (A4 = 71)
	 60,  56,  53,  50,  47,  44,  42,  39,  37,  35,  33,  31		// C5..B5
};

volatile byte senv_writes[4];

// Main code fills the pending record while cmd is zero, then publishes cmd.
static volatile byte sv_cmd[4];
static volatile byte pa_audf[4], pa_dist[4], pa_a[4], pa_d[4], pa_r[4], pa_arest[4], pa_drest[4];
static volatile word pa_p[4], pa_s[4], pa_abase[4], pa_dbase[4];

// Active envelope state. The tick updates it and the getters read it.
static volatile byte sv_state[4], sv_dist[4], sv_audc[4], sv_n[4], sv_k[4], sv_rest[4], sv_acc[4], sv_r[4];
static volatile byte sv_d[4], sv_drest[4];
static volatile word sv_q[4], sv_base[4], sv_s[4], sv_dbase[4];
static volatile bool sv_force[4];

static volatile word sv_oldvbi;			// Previous immediate VBI vector.
#pragma align(sv_oldvbi, 2)				// Avoid the NMOS 6502 indirect-jump page wrap.
static volatile byte sv_lo, sv_hi;		// Address bytes for SETVBV.
static bool sv_running;

// The OS has saved A, X, and Y. Preserve compiler scratch bytes outside
// the tick interrupt prologue, select binary arithmetic, then chain.
__asm senv_vbi
{
		lda	__tmpy
		pha
		lda	__tmp + 4
		pha
		lda	__tmp + 5
		pha
		lda	__tmp + 6
		pha
		lda	__tmp + 7
		pha
		lda	__tmp + 8
		pha
		cld
		jsr	senv_tick
		pla
		sta	__tmp + 8
		pla
		sta	__tmp + 7
		pla
		sta	__tmp + 6
		pla
		sta	__tmp + 5
		pla
		sta	__tmp + 4
		pla
		sta	__tmpy
		jmp	(sv_oldvbi)
}

// Cover base * n + rest units over n ticks.
static inline void seg(byte c, byte n, word base, byte rest)
{
	sv_n[c] = n;
	sv_k[c] = 0;
	sv_base[c] = base;
	sv_rest[c] = rest;
	sv_acc[c] = 0;
}

// Advance one tick toward the target and report when the segment ends.
static bool step(byte c, bool up)
{
	word d = sv_base[c];
	byte n = sv_n[c], r = sv_rest[c], a = sv_acc[c];
	if (a >= (byte)(n - r))			// Test a + r >= n without byte overflow.
	{
		a -= n - r;
		d++;
	}
	else
		a += r;
	sv_acc[c] = a;
	if (up)
		sv_q[c] += d;
	else
		sv_q[c] -= d;
	return ++sv_k[c] == n;
}

// Start decay or jump straight to sustain when its duration is zero.
static void to_decay(byte c)
{
	if (sv_d[c])
	{
		sv_state[c] = SENV_DECAY;
		seg(c, sv_d[c], sv_dbase[c], sv_drest[c]);
	}
	else
	{
		sv_q[c] = sv_s[c];
		sv_state[c] = SENV_SUSTAIN;
	}
}

static void advance(byte c)
{
	switch (sv_state[c])
	{
	case SENV_ATTACK:
		if (step(c, true))
			to_decay(c);
		break;
	case SENV_DECAY:
		if (step(c, false))
			sv_state[c] = SENV_SUSTAIN;
		break;
	case SENV_RELEASE:
		if (step(c, false))
			sv_state[c] = SENV_IDLE;
		break;
	}
}

__interrupt void senv_tick(void)
{
	for (byte c = 0; c < 4; c++)
	{
		byte cmd = sv_cmd[c];
		if (cmd == CMD_ON)
		{
			sv_dist[c] = pa_dist[c];
			sv_s[c] = pa_s[c];
			sv_d[c] = pa_d[c];
			sv_dbase[c] = pa_dbase[c];
			sv_drest[c] = pa_drest[c];
			sv_r[c] = pa_r[c];
			pokey.aud[c].audf = pa_audf[c];
			sv_force[c] = true;
			if (pa_a[c])
			{
				sv_q[c] = 0;
				sv_state[c] = SENV_ATTACK;
				seg(c, pa_a[c], pa_abase[c], pa_arest[c]);
				advance(c);
			}
			else
			{
				sv_q[c] = pa_p[c];
				to_decay(c);
				if (sv_state[c] == SENV_DECAY)
					advance(c);
			}
			sv_cmd[c] = 0;
		}
		else if (cmd == CMD_OFF)
		{
			byte st = sv_state[c];
			if (st != SENV_IDLE && st != SENV_RELEASE)
			{
				byte r = sv_r[c];
				if (r)
				{
					word q = sv_q[c];
					sv_state[c] = SENV_RELEASE;
					seg(c, r, q / r, (byte)(q % r));
					advance(c);
				}
				else
				{
					sv_q[c] = 0;
					sv_state[c] = SENV_IDLE;
				}
			}
			else
				advance(c);			// OFF during RELEASE still advances the current segment.
			sv_cmd[c] = 0;
		}
		else
			advance(c);

		byte st = sv_state[c];
		if (st != SENV_IDLE || sv_audc[c] != (sv_dist[c] << 4))
		{
			byte v = (sv_dist[c] << 4) | (byte)(sv_q[c] >> 8);
			if (v != sv_audc[c] || sv_force[c])
			{
				pokey.aud[c].audc = v;
				sv_audc[c] = v;
				sv_force[c] = false;
				senv_writes[c]++;
			}
		}
	}
}

static void wait_cmd(byte c)
{
	while (sv_cmd[c])
		;
}

void senv_on(byte ch, byte audf, byte dist, byte peak, const struct SEnv * e)
{
	byte c = ch & 3;
	byte a = e->attack, d = e->decay, r = e->release, sus = e->sustain;
	word p = (word)peak << 8;
	word s = (word)(p * sus) / 15;
	wait_cmd(c);
	pa_audf[c] = audf;
	pa_dist[c] = dist;
	pa_p[c] = p;
	pa_s[c] = s;
	pa_a[c] = a;
	pa_d[c] = d;
	pa_r[c] = r;
	if (a)
	{
		pa_abase[c] = p / a;
		pa_arest[c] = (byte)(p % a);
	}
	if (d)
	{
		pa_dbase[c] = (word)(p - s) / d;
		pa_drest[c] = (byte)((word)(p - s) % d);
	}
	sv_cmd[c] = CMD_ON;
}

void senv_off(byte ch)
{
	byte c = ch & 3;
	wait_cmd(c);
	sv_cmd[c] = CMD_OFF;
}

byte senv_state(byte ch)
{
	return sv_state[ch & 3];
}

byte senv_vol(byte ch)
{
	return (byte)(sv_q[ch & 3] >> 8);
}

static void setvbv_immediate(void)
{
	__asm {
		lda	#SETVBV_IMMEDIATE
		ldy	sv_lo
		ldx	sv_hi
		jsr	OS_SETVBV
	}
}

void senv_init(void)
{
	if (sv_running)
		return;
	// Save the immediate VBI vector for chaining and restoration.
	sv_oldvbi = (word)os_vvblki;
	sv_lo = (byte)(word)senv_vbi;
	sv_hi = (byte)((word)senv_vbi >> 8);
	setvbv_immediate();
	sv_running = true;
}

void senv_shutdown(void)
{
	if (!sv_running)
		return;
	sv_lo = (byte)sv_oldvbi;
	sv_hi = (byte)(sv_oldvbi >> 8);
	setvbv_immediate();
	sv_running = false;
	for (byte c = 0; c < 4; c++)
	{
		sv_cmd[c] = 0;
		if (sv_state[c] != SENV_IDLE)
		{
			sv_state[c] = SENV_IDLE;
			sv_q[c] = 0;
			pokey.aud[c].audc = 0;
		}
	}
}
