#include "irq.h"
#include "antic.h"
#include "os.h"

#define OS_SETVBV	0xe45c
#define OS_XITVBV	0xe462
#define SETVBV_DEFERRED	7

static IRQHandler vbi_user;

// The OS enters this stub through VVBLKD. Exit through XITVBV.
__asm vbi_stub
{
		jsr	vbi_tramp
		jmp	OS_XITVBV
vbi_tramp:
		jmp	(vbi_user)
}

void dli_set(IRQHandler f)
{
	antic.nmien = NMIEN_VBI;
	os_vdslst = (void *)f;
	antic.nmien = NMIEN_VBI | NMIEN_DLI;
}

void dli_off(void)
{
	antic.nmien = NMIEN_VBI;
}

static void setvbv_deferred(word addr)
{
	byte lo = (byte)addr, hi = (byte)(addr >> 8);
	__asm {
		lda	#SETVBV_DEFERRED
		ldy	lo
		ldx	hi
		jsr	OS_SETVBV
	}
}

void vbi_set(IRQHandler f)
{
	vbi_user = f;
	setvbv_deferred((word)vbi_stub);
}

void vbi_off(void)
{
	setvbv_deferred(OS_XITVBV);
}
