#include "romram.h"
#include "pia.h"
#include "antic.h"
#include "os.h"

static volatile byte rr_live;
static byte rr_old_vectors[6],rr_saved_p,rr_saved_pb,rr_nmien;

// VBI and IRQ save registers and PORTB on the hardware stack for each
// interrupt. With ROM visible, enter the OS handler using an RTI frame
// that preserves the original P, including the I flag for deferred VBI.
// The OS returns here before the final RTI to the interrupted code.
__asm rr_return {
 sei
 pla
 sta 0xd301
 pla
 tay
 pla
 tax
 pla
 rti
}
// DLI follows the XL OS dispatch sequence through VDSLST with the
// interrupted ROM mapping still in effect.
__asm rr_nmi {
 bit 0xd40f
 bpl rr_vbi
 jmp (0x0200)
rr_vbi:
 pha
 txa
 pha
 tya
 pha
 lda 0xd301
 pha
 ora #1
 sta 0xd301
 lda #>rr_return
 pha
 lda #<rr_return
 pha
 tsx
 lda 0x0107,x
 pha
 tsx
 // Keep X at the frame base while recovering the saved registers.
 lda 0x0107,x
 pha
 lda 0x0106,x
 pha
 ldy 0x0105,x
 pla
 tax
 pla
 jmp (0xfffa)
}
__asm rr_irq {
 pha
 txa
 pha
 tya
 pha
 lda 0xd301
 pha
 ora #1
 sta 0xd301
 lda #>rr_return
 pha
 lda #<rr_return
 pha
 tsx
 lda 0x0107,x
 pha
 tsx
 lda 0x0107,x
 pha
 lda 0x0106,x
 pha
 ldy 0x0105,x
 pla
 tax
 pla
 jmp (0xfffe)
}

// Allow a latched NMI to finish before switching the RAM vectors.
static void critical_enter(void)
{
 __asm volatile {
  php
  pla
  sta rr_saved_p
  sei
  lda #0
  sta 0xd40e
  nop
  nop
 }
}
static void critical_leave(void)
{
 antic.nmien=rr_nmien;
 __asm volatile { lda rr_saved_p; pha; plp }
}
static bool mask_valid(byte mask)
{
 return mask==0 || mask==NMIEN_VBI || mask==(NMIEN_VBI|NMIEN_DLI);
}
bool rr_install(byte desired_nmien)
{
 if(!mask_valid(desired_nmien))return false;
 if(rr_live)return true;
 byte ctl=pia.pbctl, pb=pia.portb;
 if(!(ctl&4) || (ctl&1) || (!(ctl&0x20) && (ctl&8)))return false;
 if((pb&0x83)!=0x83)return false;
 word dli=(word)os_vdslst;
 if((desired_nmien&NMIEN_DLI) && (!dli || dli>=0xc000))return false;
 rr_saved_pb=pb;rr_nmien=desired_nmien;
 critical_enter();
 pia.portb=rr_saved_pb&0xfe;
 if(pia.portb&1) {
  pia.portb=rr_saved_pb;critical_leave();return false;
 }
 volatile byte *vectors=(volatile byte *)0xfffa;
 for(byte i=0;i<6;i++)rr_old_vectors[i]=vectors[i];
 *((volatile word *)0xfffa)=(word)rr_nmi;
 *((volatile word *)0xfffe)=(word)rr_irq;
 bool ok=*((volatile word *)0xfffa)==(word)rr_nmi && *((volatile word *)0xfffe)==(word)rr_irq;
 if(!ok)for(byte i=0;i<6;i++)vectors[i]=rr_old_vectors[i];
 pia.portb=rr_saved_pb;
 if(ok)rr_live=1;
 critical_leave();
 return ok;
}
__dynstack __noinline byte rr_open(void)
{
 byte token=pia.portb;
 if(rr_live)pia.portb=token&0xfe;
 return token;
}
__dynstack __noinline void rr_close(byte token) { pia.portb=token; }
bool rr_active(void) { return rr_live!=0; }
bool rr_uninstall(byte desired_nmien)
{
 if(!mask_valid(desired_nmien))return false;
 if(!rr_live)return true;
 byte pb=pia.portb;
 if(!(pb&1))return false;
 rr_saved_pb=pb;rr_nmien=desired_nmien;
 critical_enter();
 pia.portb=rr_saved_pb&0xfe;
 for(byte i=0;i<6;i++)((volatile byte *)0xfffa)[i]=rr_old_vectors[i];
 pia.portb=rr_saved_pb;rr_live=0;
 critical_leave();return true;
}
