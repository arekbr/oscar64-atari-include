#include "pmdirect.h"
#include "antic.h"
#include "gtia.h"
#include "os.h"
#if PMD_SAME_INTERVAL
static const byte pmd_pages[5]={4,5,6,7,3};
#else
static const word pmd_offsets[5]={0x400,0x500,0x600,0x700,0x300};
#endif
static byte pmd_bases(const struct PMDState *s)
{
 return s&&s->base[0]&&s->base[1]&&!((word)s->base[0]&2047)&&!((word)s->base[1]&2047)&&s->base[0]!=s->base[1]&&s->active<2;
}
byte pmd_init(struct PMDState *s,volatile byte *first,volatile byte *second)
{
 if(!s||!first||!second||first==second||((word)first&2047)||((word)second&2047))return PMB_BAD;
 for(word j=0x300;j<0x800;j++){first[j]=0;second[j]=0;}
 s->base[0]=first;s->base[1]=second;
 for(byte b=0;b<2;b++)for(byte i=0;i<5;i++)s->old_y[b][i]=s->old_h[b][i]=0;
 s->active=0;
 return PMB_OK;
}
static __noinline void pmd_rows(volatile byte *d,const byte *source,byte h,byte mask)
{
#if PMD_UNROLL_ROWS
 if(h==15) {
#pragma unroll(full)
  for(byte r=0;r<15;r++) {
   d[r]=source[r]&mask;
  }
  return;
 }
#endif
 for(byte r=0;r<h;r++) {
  d[r]=source[r]&mask;
 }
}
#if PMD_CLEAR_DIFF
// d points at the first old row. Clear the portions of the old interval
// before and after the new interval. An empty new interval clears all old
// rows. Heights are at most 15, so overlap lengths fit in a byte.
static __noinline void pmd_clear(volatile byte *d,byte oy,byte oh,byte ny,byte nh)
{
 byte lo=oh,hi=oh;
 if(nh) {
  if(ny>=oy) {
   byte k=ny-oy;
   if(k<oh){lo=k;k+=nh;if(k<oh)hi=k;}
  } else {
   byte k=oy-ny;
   lo=0;
   if(k<nh){k=nh-k;hi=k<oh?k:oh;}else hi=0;
  }
 }
 for(byte r=0;r<lo;r++)d[r]=0;
 for(byte r=hi;r<oh;r++)d[r]=0;
}
#endif
static __noinline void pmd_plane(struct PMDState *s,const struct PMDLayout *l,byte i,int top,int bottom)
{
 byte back=s->active^1;
#if PMD_SAME_INTERVAL
 int y=l->y[i];byte h=l->height[i],skip=0;
 if(!h||y>=bottom||y<=top-15){y=0;h=0;}
 else {
  if(y<top){skip=(byte)(top-y);if(skip>=h){y=0;h=0;}else{y=top;h-=skip;}}
  if(h&&y+h>bottom)h=(byte)(bottom-y);
 }
 byte old_y=s->old_y[back][i],old_h=s->old_h[back][i];
 if(old_y!=(byte)y||old_h!=h)
#if PMD_CLEAR_DIFF
 if(old_h)pmd_clear(s->base[back]+((word)pmd_pages[i]<<8)+old_y,old_y,old_h,(byte)y,h);
#else
 {
  volatile byte *d=s->base[back]+((word)pmd_pages[i]<<8)+old_y;
  for(byte r=0;r<old_h;r++)d[r]=0;
 }
#endif
 s->old_y[back][i]=(byte)y;s->old_h[back][i]=h;
 if(h)pmd_rows(s->base[back]+((word)pmd_pages[i]<<8)+(word)y,l->rows[i]+skip,h,l->horizontal[i]);
#else
  volatile byte *d=s->base[back]+pmd_offsets[i]+s->old_y[back][i];
  for(byte r=0;r<s->old_h[back][i];r++)d[r]=0;
  int y=l->y[i];byte h=l->height[i],skip=0;
  if(!h||y>=bottom||y<=top-15){s->old_y[back][i]=s->old_h[back][i]=0;return;}
  if(y<top){skip=(byte)(top-y);if(skip>=h){s->old_y[back][i]=s->old_h[back][i]=0;return;}y=top;h-=skip;}
  if(y+h>bottom)h=(byte)(bottom-y);
  d=s->base[back]+pmd_offsets[i]+(word)y;
  s->old_y[back][i]=(byte)y;s->old_h[back][i]=h;
  pmd_rows(d,l->rows[i]+skip,h,l->horizontal[i]);
#endif
}
__noinline byte pmd_prepare(struct PMDState *s,struct PMDPacket *p,const struct PMDLayout *l,int top,int bottom)
{
 if(!p||!l||top<0||bottom>256||top>=bottom||!pmd_bases(s))return PMB_BAD;
 byte back=s->active^1;
 for(byte i=0;i<5;i++)if(l->height[i]>15||(l->height[i]&&!l->rows[i])||s->old_h[back][i]>15||(word)s->old_y[back][i]+s->old_h[back][i]>256)return PMB_BAD;
 p->regs=l->regs;
 p->buffer=back;
 for(byte i=0;i<5;i++) {
#if PMD_SKIP_EMPTY
  // An empty plane only needs its stored position reset.
  if(!l->height[i]&&!s->old_h[back][i]){s->old_y[back][i]=0;continue;}
#endif
  pmd_plane(s,l,i,top,bottom);
 }
 return PMB_OK;
}
PMB_ASYNC __noinline byte pmd_commit(struct PMDState *s,const struct PMDPacket *p)
{
 if(!p||!pmd_bases(s)||p->buffer>1)return PMB_BAD;
 for(byte i=0;i<4;i++) {
  gtia.hposp[i]=p->regs.hposp[i];gtia.hposm[i]=p->regs.hposm[i];gtia.sizep[i]=p->regs.sizep[i];
  gtia.colpm[i]=p->regs.colpm[i];os_pcolr[i]=p->regs.colpm[i];
 }
 gtia.sizem=p->regs.sizem;
 gtia.prior=p->regs.prior;os_gprior=p->regs.prior;
 gtia.colpf[3]=p->regs.colpf3;os_color[3]=p->regs.colpf3;
 antic.pmbase=(byte)((word)s->base[p->buffer]>>8);
 gtia.gractl=GRACTL_PLAYERS|GRACTL_MISSILES;
 s->active=p->buffer;
 return PMB_OK;
}
