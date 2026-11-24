#ifndef ATARI_ROMRAM_H
#define ATARI_ROMRAM_H
#include "types.h"
// RAM under the OS ROM on a stock 64 KB XL/XE. Start with the OS mapped,
// BASIC and self-test disabled, PORTB in data mode with bit 0 configured as
// an output, and PIA port B interrupts disabled.
//
// Keep executable code, state, stacks and all active ANTIC data below
// $C000. This includes DLI handlers, fonts and display lists. With ROM
// hidden, the module temporarily restores it for ordinary OS VBI and IRQ
// dispatch. DLI dispatch follows VDSLST directly. Keep direct ROM calls,
// CIO, SIO and BRK outside open windows.
//
// The caller owns the RAM in the two ranges below. $D000-$D7FF remains
// hardware I/O. The module uses the RAM vectors at $FFFA-$FFFF.
#define RR_LOW ((volatile byte *)0xc000)
#define RR_LOW_SIZE 4096u
#define RR_HIGH ((volatile byte *)0xd800)
#define RR_HIGH_SIZE 10234u
// Install the RAM interrupt handlers in a short critical section. Use
// desired_nmien = 0, $40 (VBI) or $C0 (VBI and DLI). The DLI vector must
// point below $C000 when DLI is enabled. Restore PORTB and the CPU I flag,
// then continue with the requested NMI mask. A valid request when already
// installed succeeds without changing the configuration.
bool rr_install(byte desired_nmien);
// Save the current PORTB value as the token and clear bit 0 if installed.
// Keep each token in the calling context. Close in reverse order so an
// inner close restores the outer mapping and the outer close restores the
// original mapping. The same order applies within wrapped interrupt
// handlers. Before installation, rr_open only reads PORTB.
__dynstack __noinline byte rr_open(void);
__dynstack __noinline void rr_close(byte token);
// Report whether the RAM interrupt handlers are installed.
bool rr_active(void);
// Close every window with its token before uninstalling, so the OS is
// mapped. Restore the six previous RAM vector bytes and apply the
// requested NMI mask, which accepts the same values as rr_install.
bool rr_uninstall(byte desired_nmien);
#pragma compile("romram.c")
#endif
