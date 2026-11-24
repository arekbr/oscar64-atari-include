#ifndef ATARI_FMT_H
#define ATARI_FMT_H

#include "types.h"
#include <stdarg.h>

// Integer and text formatting for Atari. Use d, i, u, x, X, o, c, s, p
// or %. Integer conversions take the h, hh and l length modifiers.
// h uses the same 16-bit argument as int. hh narrows to eight bits and
// l takes a 32-bit long. %p writes 0x followed by four hex digits.
//
// The flags are -, +, space, 0 and #. Width and precision can be decimal
// numbers or * arguments of type int. A negative width left-aligns the
// field. A negative precision is treated as omitted. Precision limits
// the number of bytes read for %s. %c writes a single byte, including NUL.
//
// Unknown and unfinished conversions are copied literally. Unknown
// conversions consume any * arguments but consume no value argument.
// All functions share static state. Call them and con_printf() only
// from code that cannot interrupt another formatting call.
//
// Both output forms return the full character count, even when the
// destination buffer is too small. They return -1 for a count or field
// larger than 32767, or for a field width of -32768. An invalid field
// size stops formatting at that field and preserves the prior output.

// Write at most size - 1 bytes and append NUL if size is nonzero.
// With size zero, buf may be null. Buffer output preserves '\n' as written.
int fmt_snprintf(char * buf, word size, const char * fmt, ...);
int fmt_vsnprintf(char * buf, word size, const char * fmt, va_list ap);

// Write to an open CIO channel and translate '\n' to ATASCII EOL ($9B).
// If a write returns a CIO status of 128 or greater, stop and return
// the negative status. Earlier bytes may already have been written.
int fmt_cprintf(byte ch, const char * fmt, ...);
int fmt_vcprintf(byte ch, const char * fmt, va_list ap);

#pragma compile("fmt.c")

#endif
