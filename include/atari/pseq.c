#include "pseq.h"
#include "pokey.h"

#define C_PLAY 1
#define C_STOP 2
#define C_TEMPO 3
#define C_FX 4
#define C_SUSPEND 5
#define C_RESUME 6
#define C_FX_MASK 7
#define C_BASE 8
#if PSQ_EXTENDED
#define PSQ_SLOTS 6
#else
#define PSQ_SLOTS 4
#endif
#define ENV_ATTACK 0
#define ENV_DECAY 1
#define ENV_HOLD 2
#define ENV_RELEASE 3

// Keep fields in byte-indexed arrays. Extended slots 0..2 hold base tracks
// and slots 3..5 hold effects. Core slot 3 saves track 2 while an effect plays.
#if PSQ_SEPARATE_BSS
#pragma bss(psq_state)
#endif
static const struct PSQEvent * first[PSQ_SLOTS], * next[PSQ_SLOTS];
static word total[PSQ_SLOTS], left[PSQ_SLOTS], loop[PSQ_SLOTS];
static byte units[PSQ_SLOTS], frame[PSQ_SLOTS], rowlen[PSQ_SLOTS], gate[PSQ_SLOTS], note[PSQ_SLOTS], volume[PSQ_SLOTS], phase[PSQ_SLOTS];
static byte peak[PSQ_SLOTS], sustain[PSQ_SLOTS], attack[PSQ_SLOTS], decay[PSQ_SLOTS], rel[PSQ_SLOTS], dist[PSQ_SLOTS];
static const struct PSQBank * data;
static volatile byte command, argument, priority;
static const struct PSQSong * volatile requested_song;
static volatile byte paused, running;
static byte tempo, fx_on, fx_priority, fx_pitch;
#if PSQ_EXTENDED
static byte fx_mask;
static volatile byte fx_generation;
static byte arp_id[6], arp_phase[6], arp_left[6], pitch_lo[6], pitch_hi[6];
static volatile byte requested_mask, controls_cmd;
static volatile struct PSQControls controls_payload;
static byte override_on[3], override_audc[3];
static word override_div[3];
static const byte voice_bit[3]={1,2,4};
// Map each logical voice to its fixed POKEY register offsets.
static const byte pitch_offset[3]={0,4,6}, control_offset[3]={3,5,7};
#endif
volatile byte psq_regs[9];
#if PSQ_SEPARATE_BSS
#pragma bss(bss)
#endif

static byte post(byte cmd, byte arg, byte prio)
{
	if (!data) return PSQ_NOT_INIT;
	if (command) return PSQ_BUSY;
	argument = arg;
	priority = prio;
	if (cmd == C_PLAY || cmd == C_FX
#if PSQ_EXTENDED
		|| cmd == C_FX_MASK || cmd == C_BASE
#endif
	) requested_song = data->songs + arg;
	command = cmd;                    // single-byte publication, always last
	return PSQ_OK;
}

__noinline byte psq_init(const struct PSQBank * bank)
{
	if (!bank || !bank->notes || !bank->instruments || !bank->songs ||
		!bank->note_count || !bank->instrument_count || !bank->song_count)
		return PSQ_BAD_BANK;
#if PSQ_EXTENDED
	if (bank->arp_count && !bank->arps) return PSQ_BAD_BANK;
	for (byte i=0;i<bank->arp_count;i++) if (!bank->arps[i].period) return PSQ_BAD_BANK;
#endif
	for (byte i = 0; i < bank->instrument_count; i++) {
		const struct PSQInstrument * in = bank->instruments + i;
		if (in->peak > 15 || in->sustain > in->peak || in->attack > 15 ||
			in->decay > 15 || in->release > 15 || in->distortion > 7)
			return PSQ_BAD_BANK;
#if PSQ_EXTENDED
		if (in->arp>bank->arp_count) return PSQ_BAD_BANK;
#endif
	}
	for (byte i = 0; i < bank->song_count; i++) {
		for (byte c = 0; c < 3; c++) {
			const struct PSQTrack * t = bank->songs[i].track + c;
			if (t->count > PSQ_MAX_EVENTS ||
				(t->loop != PSQ_NO_LOOP && t->loop >= t->count))
				return PSQ_BAD_BANK;
			if (t->count && !t->events) return PSQ_BAD_BANK;
			// A stream cannot cross the 16-bit CPU address space. Storage
			// for declared count records remains the program's responsibility.
			word bytes = t->count << 2;
			if (t->count && (word)t->events > (word)(0xffffu - bytes))
				return PSQ_BAD_BANK;
			for (word j = 0; j < t->count; j++) {
				const struct PSQEvent * e = t->events + j;
				if (e->note > bank->note_count || e->instrument >= bank->instrument_count ||
					!e->duration || e->gate > e->duration) return PSQ_BAD_BANK;
#if PSQ_EXTENDED
				byte a=bank->instruments[e->instrument].arp;
				if (e->note && a) for(byte k=0;k<4;k++) {
					int n=(int)e->note+bank->arps[a-1].offset[k];
					if (n<1 || n>bank->note_count) return PSQ_BAD_BANK;
				}
#endif
			}
		}
	}
	data = bank;
	command = 0;
	argument = priority = 0;
	paused = running = 0;
	fx_on = fx_priority = 0;
	tempo = 1;
	for (byte c = 0; c < PSQ_SLOTS; c++) {
		left[c] = total[c] = 0;
		loop[c] = PSQ_NO_LOOP;
		units[c] = frame[c] = gate[c] = note[c] = volume[c] = 0;
		peak[c] = sustain[c] = attack[c] = decay[c] = rel[c] = 0;
		dist[c] = 0xa0;
		phase[c] = ENV_HOLD;
#if PSQ_EXTENDED
		arp_id[c]=arp_phase[c]=arp_left[c]=pitch_lo[c]=pitch_hi[c]=0;
#endif
	}
#if PSQ_EXTENDED
	fx_mask=controls_cmd=requested_mask=0;
	fx_generation=0;
	for(byte c=0;c<3;c++) override_on[c]=override_audc[c]=0;
#endif
	for (byte c = 0; c < 9; c++) psq_regs[c] = 0;
	psq_regs[8] = 0x51;
	return PSQ_OK;
}

__noinline byte psq_play(byte song)
{
	if (!data) return PSQ_NOT_INIT;
	if (song >= data->song_count) return PSQ_BAD_SONG;
	return post(C_PLAY, song, 0);
}
__noinline byte psq_stop(void) { return post(C_STOP, 0, 0); }
__noinline byte psq_tempo(byte frames_per_unit)
{
	if (!frames_per_unit) return PSQ_BAD_TEMPO;
	return post(C_TEMPO, frames_per_unit, 0);
}
#if PSQ_EXTENDED
__noinline byte psq_fx(byte song, byte prio) { return psq_fx_mask(song,prio,4); }
__noinline byte psq_fx_mask(byte song, byte prio, byte mask)
{
	if (!data) return PSQ_NOT_INIT;
	if (song >= data->song_count) return PSQ_BAD_SONG;
	if (!mask || mask>7) return PSQ_BAD_MASK;
	if (command) return PSQ_BUSY;
	requested_mask=mask;
	return post(mask==4 ? C_FX : C_FX_MASK,song,prio);
}
__noinline byte psq_fx_generation(void) { return fx_generation; }
__noinline byte psq_base(byte song, byte mask)
{
	if (!data) return PSQ_NOT_INIT;
	if (song>=data->song_count) return PSQ_BAD_SONG;
	if (!mask || mask>7) return PSQ_BAD_MASK;
	if (command) return PSQ_BUSY;
	requested_mask=mask;
	return post(C_BASE,song,0);
}
__noinline byte psq_controls(const struct PSQControls * p)
{
	if (!data) return PSQ_NOT_INIT;
	if (!p || p->mask>7) return PSQ_BAD_CONTROL;
	for(byte c=0;c<3;c++) if (p->mask & voice_bit[c]) {
		if (p->voice[c].enabled>1 || (p->voice[c].enabled && c && p->voice[c].divider>255))
			return PSQ_BAD_CONTROL;
	}
	if (controls_cmd) return PSQ_BUSY;
	for(byte i=0;i<sizeof(struct PSQControls);i++)
		((volatile byte *)&controls_payload)[i]=((const byte *)p)[i];
	controls_cmd=1;
	return PSQ_OK;
}
__noinline byte psq_controls_pending(void) { return controls_cmd; }
#else
__noinline byte psq_fx(byte song, byte prio)
{
	if (!data) return PSQ_NOT_INIT;
	if (song >= data->song_count) return PSQ_BAD_SONG;
	return post(C_FX, song, prio);
}
#endif
__noinline byte psq_suspend(void) { return post(C_SUSPEND, 0, 0); }
__noinline byte psq_resume(void) { return post(C_RESUME, 0, 0); }
__noinline byte psq_playing(void) { return running || command == C_PLAY; }
__noinline byte psq_suspended(void) { return paused; }
__noinline byte psq_pending(void) { return command != 0; }

#if PSQ_EXTENDED
#define RESET_ARP(c) arp_id[c]=0
#else
#define RESET_ARP(c)
#endif
// Expand these operations directly in the asynchronous caller so the
// compiler does not introduce function calls in the timed playback path.
#define START(c, track_ptr) do { \
	const struct PSQTrack * st=(track_ptr); \
	first[c]=next[c]=st->events; left[c]=total[c]=st->count; loop[c]=st->loop; \
	units[c]=frame[c]=gate[c]=note[c]=volume[c]=0; phase[c]=ENV_HOLD; RESET_ARP(c); \
} while (0)

#if PSQ_EXTENDED
#define PITCH(c,channel,index) do { \
	const struct PSQNote * pn=data->notes+(index)-1; \
	if (!(channel)) { pitch_lo[c]=(byte)pn->lead; pitch_hi[c]=(byte)(pn->lead>>8); } \
	else pitch_lo[c]=(channel)==1 ? pn->bass : pn->arp; \
} while (0)

#else
static PSQ_ASYNC void save_fx(void)
{
	fx_pitch = psq_regs[6];
	first[3]=first[2]; next[3]=next[2]; total[3]=total[2]; left[3]=left[2]; loop[3]=loop[2];
	units[3]=units[2]; frame[3]=frame[2]; rowlen[3]=rowlen[2]; gate[3]=gate[2];
	note[3]=note[2]; volume[3]=volume[2]; phase[3]=phase[2];
	peak[3]=peak[2]; sustain[3]=sustain[2]; attack[3]=attack[2];
	decay[3]=decay[2]; rel[3]=rel[2]; dist[3]=dist[2];
}
static PSQ_ASYNC void restore_fx(void)
{
	psq_regs[6] = fx_pitch;
	first[2]=first[3]; next[2]=next[3]; total[2]=total[3]; left[2]=left[3]; loop[2]=loop[3];
	units[2]=units[3]; frame[2]=frame[3]; rowlen[2]=rowlen[3]; gate[2]=gate[3];
	note[2]=note[3]; volume[2]=volume[3]; phase[2]=phase[3];
	peak[2]=peak[3]; sustain[2]=sustain[3]; attack[2]=attack[3];
	decay[2]=decay[3]; rel[2]=rel[3]; dist[2]=dist[3];
}

#endif
#define RELEASE(c) do { phase[c]=ENV_RELEASE; if (!rel[c]) volume[c]=0; } while (0)

static PSQ_ASYNC void fetch(byte c
#if PSQ_EXTENDED
	, byte channel
#endif
)
{
	if (!left[c] && loop[c] != PSQ_NO_LOOP && total[c]) {
		next[c] = first[c] + loop[c]; left[c] = total[c] - loop[c];
	}
	if (!left[c]) {
		if (note[c]) RELEASE(c);
		note[c] = 0; RESET_ARP(c);
		return;
	}
	const struct PSQEvent * e = next[c]++;
	left[c]--;
	note[c] = e->note;
	units[c] = e->duration;
#if PSQ_EXTENDED
	rowlen[c]=frame[c]=(fx_mask & voice_bit[channel]) ? 1 : tempo;
#else
	rowlen[c] = frame[c] = (c == 2 && fx_on) ? 1 : tempo;
#endif
	gate[c] = e->gate;
	const struct PSQInstrument * in = data->instruments + e->instrument;
	peak[c]=in->peak; sustain[c]=in->sustain; attack[c]=in->attack;
	decay[c]=in->decay; rel[c]=in->release; dist[c]=in->distortion << 5;
#if PSQ_EXTENDED
	arp_id[c]=in->arp;
	if (!note[c]) { arp_id[c]=0; RELEASE(c); return; }
	byte n=note[c];
	if (arp_id[c]) {
		const struct PSQArp * a=data->arps+arp_id[c]-1;
		arp_phase[c]=0; arp_left[c]=a->period;
		n+=a->offset[0];
	}
	PITCH(c,channel,n);
#else
	if (!note[c]) { RELEASE(c); return; }
	const struct PSQNote * n = data->notes + note[c] - 1;
	if (!c) { psq_regs[0]=(byte)n->lead; psq_regs[2]=(byte)(n->lead >> 8); }
	else psq_regs[2+2*c] = c == 1 ? n->bass : n->arp;
#endif
	volume[c] = attack[c] ? 0 : peak[c];
	phase[c] = attack[c] ? ENV_ATTACK : ENV_DECAY;
}

PSQ_ASYNC __noinline void psq_tick(void)
{
	if (!data) return;
#if PSQ_EXTENDED
	if (controls_cmd) {
		byte mask=controls_payload.mask;
// Three fixed hardware voices: constant offsets avoid runtime field strides.
#define SET_OVERRIDE(c) override_on[c]=controls_payload.voice[c].enabled
#define CONTROL(c,bit) do { if (mask & (bit)) { \
	SET_OVERRIDE(c); override_audc[c]=controls_payload.voice[c].audc; \
	override_div[c]=controls_payload.voice[c].divider; \
} } while (0)
		CONTROL(0,1); CONTROL(1,2); CONTROL(2,4);
#undef CONTROL
#undef SET_OVERRIDE
		if (controls_payload.tempo) tempo=controls_payload.tempo;
		controls_cmd=0;
	}
#endif
	byte cmd=command;
	if (cmd) {
		byte a=argument, p=priority;
#if PSQ_EXTENDED
		byte mask=(cmd==C_FX) ? 4 : requested_mask;
#endif
		const struct PSQSong * song=requested_song;
		command=0; // payload copied before slot is released
		if (cmd==C_PLAY) {
			const struct PSQTrack * tracks= song->track;
			START(0,tracks); START(1,tracks+1); START(2,tracks+2);
			fx_on=0;
			running=1;
#if PSQ_EXTENDED
			fx_mask=0;
			for(byte c=0;c<3;c++) override_on[c]=0;
#endif
		} else if (cmd==C_STOP) {
			for (byte c=0;c<3;c++) {
				left[c]=total[c]=0; loop[c]=PSQ_NO_LOOP;
				units[c]=frame[c]=gate[c]=note[c]=volume[c]=0;
				phase[c]=ENV_HOLD; dist[c]=0xa0;
			}
			fx_on=0; running=0;
#if PSQ_EXTENDED
			fx_mask=0;
			for(byte c=0;c<3;c++) {
				override_on[c]=0;
				arp_id[c]=0;
			}
#endif
		} else if (cmd==C_TEMPO) tempo=a;
#if PSQ_EXTENDED
		else if (cmd==C_BASE) {
			for(byte c=0;c<3;c++) if (mask & voice_bit[c]) {
				START(c,song->track+c);
			}
		}
		else if ((cmd==C_FX || cmd==C_FX_MASK) && (!fx_mask || p>=fx_priority)) {
			fx_generation++;
			for(byte c=0;c<3;c++) if (mask & voice_bit[c]) {
				START(c+3,song->track+c);
			}
			fx_mask=mask; fx_priority=p;
		}
#else
		else if (cmd==C_FX && (!fx_on || p>=fx_priority)) {
			if (!fx_on) save_fx();
			START(2,song->track+2);
			fx_priority=p; fx_on=1;
		}
#endif
		else if (cmd==C_SUSPEND) {
			for (byte c=0;c<4;c++) pokey.aud[c].audc=0;
			paused=1;
		} else if (cmd==C_RESUME) paused=0;
	}
	if (paused) return;
#if PSQ_EXTENDED
	byte fx_live=0;
	for(byte channel=0;channel<3;channel++) {
		byte bit=voice_bit[channel];
		byte c=(fx_mask & bit) ? channel+3 : channel;
		if (units[c] && !--frame[c]) {
			frame[c]=rowlen[c]; units[c]--;
			if (gate[c] && !--gate[c]) RELEASE(c);
		}
		if (!units[c]) fetch(c,channel);
		else if (arp_id[c] && !--arp_left[c]) {
			const struct PSQArp * a=data->arps+arp_id[c]-1;
			arp_phase[c]=(arp_phase[c]+1)&3;
			arp_left[c]=a->period;
			byte n=note[c];
			n+=a->offset[arp_phase[c]];
			PITCH(c,channel,n);
		}
		if (units[c] || volume[c]) fx_live |= bit;
	}
	if (fx_mask && !(fx_mask & fx_live)) fx_mask=0;
#endif
	running=0;
	for (byte channel=0;channel<3;channel++) {
#if PSQ_EXTENDED
		byte c=(fx_mask & voice_bit[channel]) ? channel+3 : channel;
#else
		byte c=channel;
#endif
#if !PSQ_EXTENDED
		if (units[c] && !--frame[c]) {
			frame[c]=rowlen[c];
			units[c]--;
			if (gate[c] && !--gate[c]) RELEASE(c);
		}
		if (!units[c]) fetch(c);
		if (c==2 && fx_on && !units[c] && !volume[c]) { restore_fx(); fx_on=0; }
#endif
		{
			byte x=volume[c], step;
			if (phase[c]==ENV_ATTACK) {
				x += attack[c];
				if (x>=peak[c]) { x=peak[c]; phase[c]=ENV_DECAY; }
			} else if (phase[c]==ENV_DECAY) {
				step=decay[c];
				if (!step || x<=sustain[c]+step) { x=sustain[c]; phase[c]=ENV_HOLD; }
				else x-=step;
			} else if (phase[c]==ENV_RELEASE) {
				step=rel[c]; x=(!step || x<=step) ? 0 : x-step;
			}
			volume[c]=x;
		}
		if (units[c] || volume[c]) running=1;
#if PSQ_EXTENDED
		byte low=pitch_lo[c], high=pitch_hi[c];
		byte audc=(channel ? dist[c] : 0xa0)|volume[c];
		if (override_on[channel] && !(fx_mask & voice_bit[channel])) {
			low=(byte)override_div[channel]; high=(byte)(override_div[channel]>>8); audc=override_audc[channel];
		}
		psq_regs[pitch_offset[channel]]=low;
		psq_regs[control_offset[channel]]=audc;
		if (!channel) psq_regs[2]=high;
#else
		psq_regs[3+2*c] = (c ? dist[c] : 0xa0) | volume[c];
#endif
	}
	psq_regs[1]=0; psq_regs[8]=0x51;
	for (byte c=0;c<9;c++) ((volatile byte *)&pokey)[c]=psq_regs[c];
}
