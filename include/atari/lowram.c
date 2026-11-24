#include "lowram.h"
#include "os.h"
#include "pia.h"
#include "antic.h"
#include "pokey.h"

static byte lr_live;

// Require data access with both port interrupt sources disabled.
static bool quiet_data(byte control)
{
 if(!(control&4) || (control&1))return false;
 if(!(control&0x20) && (control&8))return false;
 return true;
}
static bool rom_handler(word address)
{
 word handler=*((volatile word *)address);
 if(handler>=0xc000 && handler<0xd000)return true;
 if(handler>=0xd800 && handler<0xfffa)return true;
 return false;
}
bool lr_takeover(void)
{
 if(lr_live)return true;
 byte port_control=pia.pbctl;
 if(!quiet_data(pia.pactl) || !quiet_data(port_control))return false;
 // Reject pending port B interrupts before a PORTB read can acknowledge them.
 if(port_control&0xc0)return false;
 byte top=os_ramtop;
 if(top!=0xa0 && top!=0xc0)return false;
 if(!rom_handler(0x0216) || !rom_handler(0x0208))return false;
 if((pia.portb&0x83)!=0x83)return false;
 // Keep the saved processor flags on the hardware stack through takeover.
 // Allow a latched NMI to finish before changing its vectors.
 __asm volatile { php; sei; lda #0; sta 0xd40e; nop; nop }
 os_sdmctl=0;antic.dmactl=0;
 *((volatile word *)0x0222)=0xe45f; // SYSVBV, the immediate VBI entry.
 *((volatile word *)0x0224)=0xe462; // XITVBV, the deferred VBI exit.
 for(byte i=0;i<10;i++)((volatile byte *)0x0218)[i]=0;
 os_pokmsk=IRQ_KEY;
 pokey.irqen=os_pokmsk;
 *((volatile byte *)0x0244)=1; // COLDST makes RESET start a cold boot.
 lr_live=1;
 __asm volatile { lda #0x40; sta 0xd40e; plp }
 return true;
}
bool lr_active(void) {return lr_live!=0;}
