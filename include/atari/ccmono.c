#include "ccmono.h"
#ifndef CCM_ASM
#define CCM_ASM 0
#endif
#if CCM_ASM != 0 && CCM_ASM != 1
#error "CCM_ASM must be0 or1"
#endif
static const byte cm_wide[16]={
 0,
 3,12,15,48,51,60,63,192,195,204,207,240,243,252,255};
#if CCM_ASM
// Assembly reads the named C arguments and locals through Oscar64's ABI.
// The row converter uses the output cache and CPU registers for its writes.
static __noinline void cm_rows(const CCImage *im,const byte *s)
{
 volatile byte *d0=im->data+7;
 byte h=im->height,phase=im->phase;
 volatile byte *d1=d0+32,*d2=phase?d0+64:d0;
 __asm volatile {
  ldy #0
  lda phase
  bne .not0
  jmp .p0
 .not0:
  cmp #1
  bne .not1
  jmp .p1
 .not1:
  cmp #2
  bne .not2
  jmp .p2
 .not2:
  jmp .p3
 .p0:
  lda (s),y
  lsr
  lsr
  lsr
  lsr
  tax
  lda cm_wide,x
  sta (d0),y
  lda (s),y
  and #15
  tax
  lda cm_wide,x
  sta (d1),y
  iny
  cpy h
  bcc .p0
  jmp .done
 .p1:
  lda (s),y
  lsr
  lsr
  lsr
  lsr
  lsr
  tax
  lda cm_wide,x
  sta (d0),y
  lda (s),y
  lsr
  and #15
  tax
  lda cm_wide,x
  sta (d1),y
  lda (s),y
  and #1
  beq .zero2
  lda #192
 .zero2:
  sta (d2),y
  iny
  cpy h
  bcc .p1
  jmp .done
 .p2:
  lda (s),y
  lsr
  lsr
  lsr
  lsr
  lsr
  lsr
  tax
  lda cm_wide,x
  sta (d0),y
  lda (s),y
  lsr
  lsr
  and #15
  tax
  lda cm_wide,x
  sta (d1),y
  lda (s),y
  and #3
  asl
  asl
  tax
  lda cm_wide,x
  sta (d2),y
  iny
  cpy h
  bcc .p2
  jmp .done
 .p3:
  lda (s),y
  and #128
  beq .zero0
  lda #3
 .zero0:
  sta (d0),y
  lda (s),y
  lsr
  lsr
  lsr
  and #15
  tax
  lda cm_wide,x
  sta (d1),y
  lda (s),y
  and #7
  asl
  tax
  lda cm_wide,x
  sta (d2),y
  iny
  cpy h
  bcc .p3
 .done:
 }
}
#else
static __noinline void cm_phase0(volatile byte *d,const byte *s,byte h)
{
 for(byte y=0;y<h;y++) {
  byte v=s[y],a=cm_wide[v>>4],b=cm_wide[v&15];
  d[y]=a;d[32+y]=b;
 }
}
static __noinline void cm_phase1(volatile byte *d,const byte *s,byte h)
{
 for(byte y=0;y<h;y++) {
  byte v=s[y],a=cm_wide[v>>4],b=cm_wide[v&15];
  d[y]=a>>2;d[32+y]=(a<<6)|(b>>2);d[64+y]=b<<6;
 }
}
static __noinline void cm_phase2(volatile byte *d,const byte *s,byte h)
{
 for(byte y=0;y<h;y++) {
  byte v=s[y],a=cm_wide[v>>4],b=cm_wide[v&15];
  d[y]=a>>4;d[32+y]=(a<<4)|(b>>4);d[64+y]=b<<4;
 }
}
static __noinline void cm_phase3(volatile byte *d,const byte *s,byte h)
{
 for(byte y=0;y<h;y++) {
  byte v=s[y],a=cm_wide[v>>4],b=cm_wide[v&15];
  d[y]=a>>6;d[32+y]=(a<<2)|(b>>6);d[64+y]=b<<2;
 }
}
#endif
static __noinline void cm_bounds(CCImage *im)
{
 const volatile byte *d=im->data;volatile byte *lo=im->lo,*hi=im->hi;
 byte stop=7+im->height;
 for(byte col=0;col<im->cols;col++,d+=32) {
  byte first=7,last=stop;
  while(first<stop&&!d[first])first++;
  if(first==stop){lo[col]=32;hi[col]=0;}
  else{do{last--;}while(!d[last]);lo[col]=first;hi[col]=last;}
 }
 if(im->cols==2&&im->capacity==3){lo[2]=32;hi[2]=0;}
}
byte cc_mono(CCImage *im,const byte *rows,byte height,byte phase)
{
 if(!im||!im->data||im->mask||!rows||!height||height>16||phase>3)return CC_BAD;
 byte cols=phase?3:2;
 if(im->capacity<cols||im->capacity>3)return CC_BAD;
 volatile byte *d=im->data;
 if(im->height>height)for(byte c=0;c<im->capacity;c++)for(byte y=7+height;y<7+im->height;y++)d[(word)c*32+y]=0;
 im->height=height;im->phase=phase;im->cols=cols;
 d+=7;
#if CCM_ASM
 cm_rows(im,rows);
 if(phase==0&&im->capacity==3)for(byte y=0;y<height;y++)d[64+y]=0;
#else
 if(phase==0){cm_phase0(d,rows,height);if(im->capacity==3)for(byte y=0;y<height;y++)d[64+y]=0;}
 else if(phase==1)cm_phase1(d,rows,height);
 else if(phase==2)cm_phase2(d,rows,height);
 else cm_phase3(d,rows,height);
#endif
 cm_bounds(im);
 return CC_OK;
}
