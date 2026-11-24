#ifndef ATARI_PMDIRECT_H
#define ATARI_PMDIRECT_H
#include "pmbatch.h"
// Optional build flags, all off by default. Use one setting throughout
// the program. SAME_INTERVAL skips a clear when clipped Y and height match
// the old interval in the inactive arena. All new rows are still written.
#ifndef PMD_SAME_INTERVAL
#define PMD_SAME_INTERVAL 0
#endif
#if PMD_SAME_INTERVAL != 0 && PMD_SAME_INTERVAL != 1
#error "PMD_SAME_INTERVAL must be 0 or 1"
#endif
// With CLEAR_DIFF, clear only old rows outside the new interval. The new
// rows overwrite the overlap, including rows whose new value is zero.
#ifndef PMD_CLEAR_DIFF
#define PMD_CLEAR_DIFF 0
#endif
#if PMD_CLEAR_DIFF != 0 && PMD_CLEAR_DIFF != 1
#error "PMD_CLEAR_DIFF must be 0 or 1"
#endif
#if PMD_CLEAR_DIFF && !PMD_SAME_INTERVAL
#error "PMD_CLEAR_DIFF requires PMD_SAME_INTERVAL=1"
#endif
// UNROLL_ROWS unrolls the copy for 15 rows. Shorter images use a loop.
#ifndef PMD_UNROLL_ROWS
#define PMD_UNROLL_ROWS 0
#endif
#if PMD_UNROLL_ROWS != 0 && PMD_UNROLL_ROWS != 1
#error "PMD_UNROLL_ROWS must be 0 or 1"
#endif
// SKIP_EMPTY bypasses a plane when both its source and old image are empty.
#ifndef PMD_SKIP_EMPTY
#define PMD_SKIP_EMPTY 0
#endif
#if PMD_SKIP_EMPTY != 0 && PMD_SKIP_EMPTY != 1
#error "PMD_SKIP_EMPTY must be 0 or 1"
#endif
// Upload images in main code and switch buffers in the caller's VBI.
// Supply two separate writable 2 KiB arenas, each aligned to 2 KiB.
// Both use the batch driver's single-line layout and preserve $000..$2FF.
// The caller owns DMA, vectors, the publication flag and acknowledgement.
struct PMDState {
 volatile byte *base[2];
 byte old_y[2][5],old_h[2][5];
 volatile byte active;
};
struct PMDPacket { volatile byte buffer;volatile struct PMBRegs regs; };
struct PMDLayout {
 const byte *rows[5];
 int y[5];
 byte height[5],horizontal[5];
 struct PMBRegs regs;
};
// Initialize from main code while the consumer and PMG DMA are inactive.
// Keep the arenas, source rows, state, packet and layout separate.
byte pmd_init(struct PMDState *state,volatile byte *first,volatile byte *second);
// Prepare from main code after the previous commit is acknowledged. Draw
// into the inactive arena, clipping to [top, bottom) within 0..256 and ANDing
// each source row with its horizontal mask. Supply height bytes per plane,
// with height 0..15. A zero-height plane may have a null source. Source rows
// stay immutable during this call. Invalid inputs leave state unchanged.
//
// Keep active fixed throughout prepare. Once preparation succeeds, publish
// the packet through a caller-owned flag and leave it immutable until VBI
// consumes it. Clear that flag to acknowledge only after commit finishes.
__noinline byte pmd_prepare(struct PMDState *state,struct PMDPacket *packet,const struct PMDLayout *layout,int top,int bottom);
// Commit in the caller's VBI with the same bridge requirements as
// pmb_commit. Switch PMBASE, update registers and shadows, then record the
// selected arena as active. Commit uses the packet and does not touch
// image rows. Repeat a packet only while it remains immutable.
// GRACTL enables players and missiles and clears the trigger-latch bit.
PMB_ASYNC __noinline byte pmd_commit(struct PMDState *state,const struct PMDPacket *packet);
#pragma compile("pmdirect.c")
#endif
