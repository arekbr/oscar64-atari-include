#include <atari/antic.h>
#include <atari/dlist.h>
#include <atari/gtia.h>
#include <atari/input.h>
#include <atari/irq.h>
#include <atari/os.h>
#include <atari/pmdirect.h>

static byte memory[4096];
#pragma align(memory, 2048)
static byte display_list[128];
#pragma align(display_list, 1024)
static byte blank_row[40];

static const byte square[] = {255, 255, 255, 255, 255, 255, 255, 255};
static struct PMDState state;
static struct PMDLayout layout;
static struct PMDPacket packet;
static volatile bool pending;
static volatile byte status;

static __dynstack __interrupt void publish(void)
{
	if (pending)
	{
		status = pmd_commit(&state, &packet);
		pending = false;
	}
}

// Save the compiler scratch bytes outside the callback's interrupt prologue.
__asm publish_bridge
{
	lda __tmpy
	pha
	lda __tmp+4
	pha
	lda __tmp+5
	pha
	lda __tmp+6
	pha
	lda __tmp+7
	pha
	lda __tmp+8
	pha
	cld
	jsr publish
	pla
	sta __tmp+8
	pla
	sta __tmp+7
	pla
	sta __tmp+6
	pla
	sta __tmp+5
	pla
	sta __tmp+4
	pla
	sta __tmpy
	rts
}

static bool show(byte x, byte y)
{
	layout.regs.hposp[0] = x;
	layout.y[0] = y;
	if (pmd_prepare(&state, &packet, &layout, 0, 256) != PMB_OK)
		return false;
	pending = true;
	while (pending)
		;
	return status == PMB_OK;
}

int main(void)
{
	os_sdmctl &= (byte)~(DMACTL_PLAYERS | DMACTL_MISSILES);
	antic.dmactl = os_sdmctl;
	if (pmd_init(&state, memory, memory + 2048) != PMB_OK)
		return 1;
	os_color[2] = 0;
	os_color[4] = 0;
	volatile byte * dl = dl_blank(display_list, 24);
	for (byte row = 0; row < 24; ++row)
		dl = dl_lms(dl, DL_MODE2, blank_row);
	dl_jvb(dl, display_list);
	dl_set(display_list);

	layout.rows[0] = square;
	layout.height[0] = sizeof(square);
	layout.horizontal[0] = 255;
	layout.regs.colpm[0] = ATARI_COLOR(3, 6);
	os_sdmctl |= DMACTL_PM_1LINE | DMACTL_PLAYERS | DMACTL_MISSILES;
	vbi_set((IRQHandler)publish_bridge);
	if (!show(90, 60))
		return 1;
	key_getc();
	if (!show(140, 96))
		return 1;
	vbi_off();
	for (;;)
		os_wait_frame();
	return 0;
}
