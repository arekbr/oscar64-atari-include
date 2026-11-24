#ifndef ATARI_PMBATCH_H
#define ATARI_PMBATCH_H
#include "types.h"
// Batch frames for single-line player/missile graphics. Supply a writable
// 2 KiB arena aligned to 2 KiB. The missile plane starts at $300 and player
// planes at $400, $500, $600, and $700. The prefix before $300 stays available
// for caller data. Each plane stores up to 15 image rows per frame.
//
// The caller configures DMA, supplies the vertical blank interrupt (VBI),
// and coordinates publication. Keep a published frame immutable until the
// consumer acknowledges it. Keep state, frames, layouts and arena separate.
#define PMB_USED_OFFSET 0x300
#define PMB_PLANES 5
#define PMB_HEIGHT 15
#define PMB_OK 0
#define PMB_BAD 1
// Set this option consistently for the whole program. The default (0)
// clears the old rows before copying each plane. With 1, an unchanged
// clipped Y and height skip the clear. Both profiles copy every new row,
// including zeros.
#ifndef PMB_SAME_INTERVAL
#define PMB_SAME_INTERVAL 0
#endif
#if PMB_SAME_INTERVAL != 0 && PMB_SAME_INTERVAL != 1
#error "PMB_SAME_INTERVAL must be 0 or 1"
#endif
#ifndef PMB_ASYNC
#define PMB_ASYNC __dynstack
#endif
struct PMBRegs {
 byte hposp[4],hposm[4],sizep[4],sizem,prior,colpm[4],colpf3;
};
struct PMBLayout { int y[5];byte height[5];struct PMBRegs regs; };
struct PMBFrame {
 volatile byte pixels[5][15]; // Players 0..3, then missiles. Supply horizontally masked rows.
 volatile byte y[5],height[5];
 volatile struct PMBRegs regs;
};
struct PMBState { volatile byte *base;byte old_y[5],old_h[5]; };
// Initialize state and clear the used arena pages from main code, before
// enabling DMA or attaching the commit consumer.
byte pmb_init(struct PMBState *state,volatile byte *arena);
// From main code, clip an unpublished frame in place to [top, bottom),
// with 0 <= top < bottom <= 256. Put the raw image rows in frame.pixels
// before calling. The function validates heights and bounds before changing
// the frame. The caller handles horizontal masking, clipping and occlusion.
__noinline byte pmb_prepare(struct PMBFrame *frame,const struct PMBLayout *layout,int top,int bottom);
// Commit an immutable frame in the caller's VBI. The bridge must preserve
// tmpy and WORK+4..8 and call through a __dynstack __interrupt callback.
// Enable single-line DMA for players and missiles before display. Commit
// updates the GTIA registers, relevant OS shadows, PMBASE and GRACTL.
// GRACTL enables players and missiles and clears the trigger-latch bit.
PMB_ASYNC __noinline byte pmb_commit(struct PMBState *state,const struct PMBFrame *frame);
#pragma compile("pmbatch.c")
#endif
