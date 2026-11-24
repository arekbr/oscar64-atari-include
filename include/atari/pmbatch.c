#include "pmbatch.h"
#include "antic.h"
#include "gtia.h"
#include "os.h"
// Unroll the common full-height copy and clear by default.
#ifndef PMB_UNROLL15
#define PMB_UNROLL15 1
#endif
static const word pmb_offsets[5]={0x400,0x500,0x600,0x700,0x300};
byte pmb_init(struct PMBState *s,volatile byte *arena)
{
 if(!s||!arena||((word)arena&2047))return PMB_BAD;
 for(word j=0x300;j<0x800;j++)arena[j]=0;
 s->base=arena;
 for(byte i=0;i<5;i++)s->old_y[i]=s->old_h[i]=0;
 return PMB_OK;
}
__noinline byte pmb_prepare(struct PMBFrame *f,const struct PMBLayout *l,int top,int bottom)
{
 if(!f||!l||top<0||bottom>256||top>=bottom)return PMB_BAD;
 for(byte i=0;i<5;i++)if(l->height[i]>15)return PMB_BAD;
 for(byte i=0;i<5;i++) {
  int y=l->y[i];byte h=l->height[i],skip=0;
  if(y>=bottom||y<=top-15){f->y[i]=f->height[i]=0;continue;}
  if(y<top){skip=(byte)(top-y);if(skip>=h){f->y[i]=f->height[i]=0;continue;}y=top;h-=skip;}
  if(y+h>bottom)h=(byte)(bottom-y);
  volatile byte *p=f->pixels[i];
  if(skip)for(byte r=0;r<h;r++)p[r]=p[r+skip];
  f->y[i]=(byte)y;f->height[i]=h;
 }
 f->regs=l->regs;
 return PMB_OK;
}
PMB_ASYNC __noinline byte pmb_commit(struct PMBState *s,const struct PMBFrame *f)
{
 if(!s||!f||!s->base||((word)s->base&2047))return PMB_BAD;
 for(byte i=0;i<5;i++)if(f->height[i]>15||s->old_h[i]>15||(word)f->y[i]+f->height[i]>256||(word)s->old_y[i]+s->old_h[i]>256)return PMB_BAD;
 volatile byte *base=s->base;
 for(byte i=0;i<5;i++) {
#if PMB_SAME_INTERVAL
  if(s->old_y[i]==f->y[i] && s->old_h[i]==f->height[i])continue;
#endif
  volatile byte *p=base+pmb_offsets[i]+s->old_y[i];byte n=s->old_h[i];
#if PMB_UNROLL15
  if(n==15){p[0]=0;p[1]=0;p[2]=0;p[3]=0;p[4]=0;p[5]=0;p[6]=0;p[7]=0;p[8]=0;p[9]=0;p[10]=0;p[11]=0;p[12]=0;p[13]=0;p[14]=0;}
  else
#endif
  for(byte r=0;r<n;r++)p[r]=0;
 }
 for(byte i=0;i<5;i++) {
  volatile byte *p=base+pmb_offsets[i]+f->y[i];byte n=f->height[i];const volatile byte *src=f->pixels[i];
#if PMB_UNROLL15
  if(n==15){p[0]=src[0];p[1]=src[1];p[2]=src[2];p[3]=src[3];p[4]=src[4];p[5]=src[5];p[6]=src[6];p[7]=src[7];p[8]=src[8];p[9]=src[9];p[10]=src[10];p[11]=src[11];p[12]=src[12];p[13]=src[13];p[14]=src[14];}
  else
#endif
  for(byte r=0;r<n;r++)p[r]=src[r];
  s->old_y[i]=f->y[i];s->old_h[i]=f->height[i];
 }
 for(byte i=0;i<4;i++) {
  gtia.hposp[i]=f->regs.hposp[i];gtia.hposm[i]=f->regs.hposm[i];gtia.sizep[i]=f->regs.sizep[i];
  gtia.colpm[i]=f->regs.colpm[i];os_pcolr[i]=f->regs.colpm[i];
 }
 gtia.sizem=f->regs.sizem;
 gtia.prior=f->regs.prior;os_gprior=f->regs.prior;
 gtia.colpf[3]=f->regs.colpf3;os_color[3]=f->regs.colpf3;
 antic.pmbase=(byte)((word)base>>8);gtia.gractl=GRACTL_PLAYERS|GRACTL_MISSILES;
 return PMB_OK;
}
