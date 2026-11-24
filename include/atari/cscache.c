#include "cscache.h"
#ifndef CC_ASM
#define CC_ASM 1
#endif
// Shared foreground workspace, kept out of DLI and VBI handlers.
static __zeropage volatile byte *c33_cell,*c33_dst;
static __zeropage const volatile byte *c33_bg;
static __zeropage const byte *c33_src,*c33_mask,*c33_src_col,*c33_mask_col;
static __zeropage volatile byte *c33_cell_col;
static __zeropage byte *c33_hist;
static __zeropage byte *c33_bounds;
static const byte *c33_lut;
static byte c33_or,c33_reuse,c33_code;
static byte c33_restore_count,c33_count,c33_off0,c33_off,c33_scol,c33_min,c33_max;
static byte c33_row0,c33_rows,c33_width,c33_slot,c33_row,c33_left,c33_bank,c33_old,c33_hi;
#if CC_ASM
static const byte *c33_tcolors,*c33_tmasks;
static byte c33_tcols,c33_oh,c33_bc,c33_base,c33_stop,c33_found;
static byte c33_tphase,c33_theight,c33_ri,c33_ry,c33_a,c33_b,c33_v0,c33_v1,c33_v2;
__asm cc_translate_native {
 lda c33_or
 beq c33_tr_masked
 lda #0
 sta c33_bound_cmp0 + 1
 sta c33_bound_cmp1 + 1
 jmp c33_tr_start
c33_tr_masked:
 lda #255
 sta c33_bound_cmp0 + 1
 sta c33_bound_cmp1 + 1
 lda c33_tcolors
 sta c33_ink0 + 1
 sta c33_ink1 + 1
 sta c33_ink2 + 1
 lda c33_tcolors + 1
 sta c33_ink0 + 2
 sta c33_ink1 + 2
 sta c33_ink2 + 2
 lda c33_tmasks
 sta c33_keep0 + 1
 sta c33_keep1 + 1
 sta c33_keep2 + 1
 lda c33_tmasks + 1
 sta c33_keep0 + 2
 sta c33_keep1 + 2
 sta c33_keep2 + 2
c33_tr_start:
 lda #0
 sta c33_ri
 lda #7
 sta c33_ry
 lda c33_tphase
 beq c33_set0
 cmp #1
 beq c33_set1
 cmp #2
 beq c33_set2
 lda #<c33_phase3
 ldx #>c33_phase3
 jmp c33_setphase
c33_set0:
 lda #<c33_phase0
 ldx #>c33_phase0
 jmp c33_setphase
c33_set1:
 lda #<c33_phase1
 ldx #>c33_phase1
 jmp c33_setphase
c33_set2:
 lda #<c33_phase2
 ldx #>c33_phase2
c33_setphase:
 sta c33_phasejump + 1
 stx c33_phasejump + 2
c33_trow:
 ldy c33_ri
 lda (c33_src),y
 sta c33_a
 iny
 lda (c33_src),y
 sta c33_b
 iny
 sty c33_ri
c33_phasejump:
 jmp c33_phase0
c33_phase0:
 lda c33_a
 sta c33_v0
 lda c33_b
 sta c33_v1
 jmp c33_emit
c33_phase1:
 lda c33_a
 lsr
 lsr
 sta c33_v0
 lda c33_a
 asl
 asl
 asl
 asl
 asl
 asl
 sta c33_v1
 lda c33_b
 lsr
 lsr
 ora c33_v1
 sta c33_v1
 lda c33_b
 asl
 asl
 asl
 asl
 asl
 asl
 sta c33_v2
 jmp c33_emit
c33_phase2:
 lda c33_a
 lsr
 lsr
 lsr
 lsr
 sta c33_v0
 lda c33_a
 asl
 asl
 asl
 asl
 sta c33_v1
 lda c33_b
 lsr
 lsr
 lsr
 lsr
 ora c33_v1
 sta c33_v1
 lda c33_b
 asl
 asl
 asl
 asl
 sta c33_v2
 jmp c33_emit
c33_phase3:
 lda c33_a
 lsr
 lsr
 lsr
 lsr
 lsr
 lsr
 sta c33_v0
 lda c33_a
 asl
 asl
 sta c33_v1
 lda c33_b
 lsr
 lsr
 lsr
 lsr
 lsr
 lsr
 ora c33_v1
 sta c33_v1
 lda c33_b
 asl
 asl
 sta c33_v2
 jmp c33_emit
c33_emit:
 lda c33_or
 beq c33_emit_masked
 ldy c33_ry
 lda c33_v0
 sta (c33_dst),y
 tya
 clc
 adc #32
 tay
 lda c33_v1
 sta (c33_dst),y
 lda c33_tphase
 beq c33_trow_done
 tya
 clc
 adc #32
 tay
 lda c33_v2
 sta (c33_dst),y
 jmp c33_trow_done
c33_emit_masked:
 ldy c33_ry
 ldx c33_v0
c33_ink0:
 lda 0xffff,x
 sta (c33_dst),y
c33_keep0:
 lda 0xffff,x
 sta (c33_mask),y
 tya
 clc
 adc #32
 tay
 ldx c33_v1
c33_ink1:
 lda 0xffff,x
 sta (c33_dst),y
c33_keep1:
 lda 0xffff,x
 sta (c33_mask),y
 lda c33_tphase
 beq c33_trow_done
 tya
 clc
 adc #32
 tay
 ldx c33_v2
c33_ink2:
 lda 0xffff,x
 sta (c33_dst),y
c33_keep2:
 lda 0xffff,x
 sta (c33_mask),y
c33_trow_done:
 inc c33_ry
 dec c33_theight
 beq c33_translate_done
 jmp c33_trow
c33_translate_done:
 lda #0
 sta c33_bc
 sta c33_base
c33_bound_col:
 clc
 lda c33_base
 adc #7
 tay
 clc
 adc c33_oh
 sta c33_stop
 lda #32
 sta c33_found
c33_bound_lo:
 lda (c33_mask),y
c33_bound_cmp0:
 cmp #255
 bne c33_bound_hit
 iny
 cpy c33_stop
 bcc c33_bound_lo
 jmp c33_bound_empty
c33_bound_hit:
 tya
 and #31
 sta c33_found
 ldy c33_stop
 dey
c33_bound_hi:
 lda (c33_mask),y
c33_bound_cmp1:
 cmp #255
 bne c33_bound_save_hi
 dey
 jmp c33_bound_hi
c33_bound_save_hi:
 tya
 and #31
 sta c33_stop
 jmp c33_bound_save
c33_bound_empty:
 lda #0
 sta c33_stop
c33_bound_save:
 ldy c33_bc
 lda c33_found
 sta (c33_bounds),y
 tya
 clc
 adc #3
 tay
 lda c33_stop
 sta (c33_bounds),y
 inc c33_bc
 lda c33_bc
 cmp c33_tcols
 beq c33_bounds_done
 clc
 lda c33_base
 adc #32
 sta c33_base
 jmp c33_bound_col
c33_bounds_done:
 rts
}
__asm cc_restore_native {
 lda c33_restore_count
 beq c33_rest_done
c33_rest:
 sec
 lda c33_hist
 sbc #3
 sta c33_hist
 bcs c33_rest_ptr
 dec c33_hist + 1
c33_rest_ptr:
 ldy #0
 lda (c33_hist),y
 sta c33_cell
 iny
 lda (c33_hist),y
 sta c33_cell + 1
 iny
 lda (c33_hist),y
 ldy #0
 sta (c33_cell),y
 dec c33_restore_count
 bne c33_rest
c33_rest_done:
 rts
}
// Blend eight scan lines per cell from a padded cache column.
// Copy directly when the background glyph is zero.
__asm cc_walk_native {
 lda c33_lut
 sta c33_dl + 1
 sta c33_bl + 1
 lda c33_lut + 1
 sta c33_dl + 2
 sta c33_bl + 2
 clc
 lda c33_lut
 adc #128
 sta c33_dh + 1
 sta c33_bh + 1
 lda c33_lut + 1
 adc #0
 sta c33_dh + 2
 sta c33_bh + 2
 lda #0
 sta c33_hi
c33_column:
 ldy c33_scol
 lda (c33_bounds),y
 sta c33_min
 tya
 clc
 adc #3
 tay
 lda (c33_bounds),y
 sta c33_max
 lda c33_off0
 sta c33_off
 lda c33_cell_col
 sta c33_cell
 lda c33_cell_col + 1
 sta c33_cell + 1
 lda c33_src_col
 sta c33_src
 lda c33_src_col + 1
 sta c33_src + 1
 lda c33_mask_col
 sta c33_mask
 lda c33_mask_col + 1
 sta c33_mask + 1
 lda c33_row0
 sta c33_row
 lda c33_rows
 sta c33_left
c33_cell_loop:
 clc
 lda c33_off
 adc #7
 cmp c33_min
 bcs c33_maybe_opaque
 jmp c33_skip
c33_maybe_opaque:
 lda c33_max
 cmp c33_off
 bcs c33_opaque
 jmp c33_skip
c33_opaque:
 ldy #0
 lda (c33_cell),y
 sta c33_old
 cmp #CC_STATIC_LIMIT
 bcc c33_first
 lda #1
 sta c33_reuse
 lda c33_old
 sta c33_code
 jmp c33_destination
c33_first:
 lda #0
 sta c33_reuse
 inc c33_count
 ldy c33_hi
 lda c33_cell
 sta (c33_hist),y
 iny
 lda c33_cell + 1
 sta (c33_hist),y
 iny
 lda c33_old
 sta (c33_hist),y
 iny
 sty c33_hi
 ldy #0
 lda c33_slot
 sta (c33_cell),y
 sta c33_code
c33_destination:
 lda c33_row
 and #3
 asl
 asl
 sta c33_bank
 ldx c33_code
c33_dl:
 lda 0xffff,x
 sta c33_dst
c33_dh:
 lda 0xffff,x
 clc
 adc c33_bank
 sta c33_dst + 1
 lda c33_reuse
 beq c33_background
 lda c33_dst
 sta c33_bg
 lda c33_dst + 1
 sta c33_bg + 1
 jmp c33_mix
c33_background:
 ldx c33_old
 bne c33_background_nonzero
 jmp c33_copy
c33_background_nonzero:
c33_bl:
 lda 0xffff,x
 sta c33_bg
c33_bh:
 lda 0xffff,x
 clc
 adc c33_bank
 sta c33_bg + 1
c33_mix:
 lda c33_or
 beq c33_masked_mix
 ldy #0
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 ora (c33_src),y
 sta (c33_dst),y
 jmp c33_cell_done
c33_masked_mix:
 ldy #0
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_bg),y
 and (c33_mask),y
 ora (c33_src),y
 sta (c33_dst),y
 jmp c33_cell_done
c33_copy:
 ldy #0
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
 iny
 lda (c33_src),y
 sta (c33_dst),y
c33_cell_done:
c33_skip:
 dec c33_left
 beq c33_next_col
 inc c33_row
 clc
 lda c33_off
 adc #8
 sta c33_off
 clc
 lda c33_src
 adc #8
 sta c33_src
 bcc c33_src_ok
 inc c33_src + 1
c33_src_ok:
 clc
 lda c33_mask
 adc #8
 sta c33_mask
 bcc c33_mask_ok
 inc c33_mask + 1
c33_mask_ok:
 clc
 lda c33_cell
 adc #40
 sta c33_cell
 bcc c33_cell_ok
 inc c33_cell + 1
c33_cell_ok:
 jmp c33_cell_loop
c33_next_col:
 dec c33_width
 beq c33_done
 inc c33_slot
 inc c33_scol
 inc c33_cell_col
 bne c33_col_ok
 inc c33_cell_col + 1
c33_col_ok:
 clc
 lda c33_src_col
 adc #32
 sta c33_src_col
 bcc c33_src_col_ok
 inc c33_src_col + 1
c33_src_col_ok:
 clc
 lda c33_mask_col
 adc #32
 sta c33_mask_col
 bcc c33_mask_col_ok
 inc c33_mask_col + 1
c33_mask_col_ok:
 jmp c33_column
c33_done:
 rts
}
#endif
static const word cc_rows[25]={0,40,80,120,160,200,240,280,320,360,400,440,480,520,560,600,640,680,720,760,800,840,880,920,960};
void cc_luts(byte *colors,byte *masks,byte skin,byte own)
{
 byte own0=(own&1)?255:0,own1=(own&2)?255:0;
 byte change0=own0^((skin&1)?255:0),change1=own1^((skin&2)?255:0);
 for(word v=0;v<256;v++) {
  // Separate the low bits of four source pixels. Code01 is opaque black;
  // code10 selects own, code11 skin. No shifts depend on a runtime count.
  byte lo=(byte)v&0x55,hi=((byte)v>>1)&0x55;
  // hi selects an opaque colored pixel; lo chooses own0/1 or skin0/1.
  byte d=(hi&((lo&change0)^own0))|((hi&((lo&change1)^own1))<<1);
  byte t=lo|hi;
  colors[v]=d;masks[v]=~(t|(t<<1));
 }
}
bool cc_init(CCState *c,const CCConfig *cfg)
{
 volatile byte *f=cfg->fonts,*s0=cfg->screen0,*s1=cfg->screen1;
 if(!f||((word)f&1023)||!s0||!s1||s0==s1||!cfg->background||!cfg->history0||!cfg->history1||!cfg->glyphs)return false;
 for(byte b=0;b<4;b++)for(byte y=0;y<8;y++)if(f[(word)b*1024+y])return false;
 for(word i=0;i<1000;i++)if(cfg->background[i]>=CC_STATIC_LIMIT)return false;
 c->fonts=f;c->screens[0]=s0;c->screens[1]=s1;c->history[0].entries=cfg->history0;c->history[1].entries=cfg->history1;
 c->history[0].count=c->history[1].count=0;c->glyphs=cfg->glyphs;c->back=c->ids=0;c->poll=nullptr;
 for(byte i=0;i<128;i++){cfg->glyphs[i]=i<<3;cfg->glyphs[128+i]=((word)f>>8)+(i>>5);}
 for(word i=0;i<1000;i++){s0[i]=cfg->background[i];s1[i]=cfg->background[i];}
 return true;
}
__noinline void cc_begin(CCState *c,byte back)
{
 c->back=back&1;c->ids=0;CCHistory *h=c->history+c->back;
#if CC_ASM
 c33_hist=(byte *)(h->entries+h->count);c33_restore_count=h->count;
 __asm { jsr cc_restore_native }
#else
 for(byte k=h->count;k;k--) {CCEntry *e=h->entries+k-1;*e->cell=e->old;}
#endif
 h->count=0;
 if(c->poll)c->poll();
}
__noinline byte cc_patch(CCState *c,const word *idx,const byte *code,byte n)
{
 if(c->ids)return CC_BAD;
 for(byte i=0;i<n;i++)if(idx[i]>=1000||code[i]>=CC_STATIC_LIMIT)return CC_BAD;
 volatile byte *s=c->screens[c->back];
 for(byte i=0;i<n;i++)s[idx[i]]=code[i];
 return CC_OK;
}
byte cc_cache_init(CCImage *im,byte capacity)
{
 if(!im->data||capacity<2||capacity>3)return CC_BAD;
 for(byte j=0;j<capacity*32;j++){im->data[j]=0;if(im->mask)im->mask[j]=255;}
 im->capacity=capacity;im->cols=im->height=im->phase=0;return CC_OK;
}
__noinline byte cc_translate(CCImage *im,const CCSource *s)
{
 if(!im->data||!s->raw||(im->mask&&(!s->colors||!s->masks))||!s->height||s->height>16||s->phase>3)return CC_BAD;
 byte cols=s->phase?3:2,n=s->phase<<1;
 if(cols>im->capacity)return CC_BAD;
 if(im->height>s->height)for(byte c=0;c<im->capacity;c++)for(byte y=7+s->height;y<7+im->height;y++){im->data[(word)c*32+y]=0;if(im->mask)im->mask[(word)c*32+y]=255;}
 im->cols=cols;im->height=s->height;im->phase=s->phase;
#if CC_ASM
 c33_src=s->raw;c33_dst=im->data;if(im->mask){c33_or=0;c33_mask=im->mask;}else{c33_or=1;c33_mask=im->data;}c33_bounds=im->lo;c33_tcols=cols;c33_oh=s->height;c33_tcolors=s->colors;c33_tmasks=s->masks;c33_tphase=s->phase;c33_theight=s->height;
 __asm { jsr cc_translate_native }
#else
 for(byte c=0;c<cols;c++) {
  byte *d=im->data+(word)c*32,*m=im->mask?im->mask+(word)c*32:nullptr;
  for(byte y=0;y<s->height;y++) {
   byte a=s->raw[y*2],b=s->raw[y*2+1],v;
   if(!n)v=c?b:a;
   else if(!c)v=a>>n;
   else if(c==1)v=(a<<(8-n))|(b>>n);
   else v=b<<(8-n);
   if(im->mask){d[7+y]=s->colors[v];m[7+y]=s->masks[v];}else d[7+y]=v;
  }
 }
#endif
#if !CC_ASM
 for(byte col=0;col<cols;col++) {
  byte lo=32,hi=0;
  for(byte r=7;r<7+s->height;r++)if(im->mask ? im->mask[(word)col*32+r]!=255 : im->data[(word)col*32+r]!=0){if(lo==32)lo=r;hi=r;}
  im->lo[col]=lo;im->hi[col]=hi;
 }
#endif
 return CC_OK;
}
__noinline byte cc_draw(CCState *c,byte id,const CCImage *im,int x,int y)
{
 if(id>=CC_MAX_ACTORS||!im->data||!im->height||im->height>16||im->phase!=((byte)x&3)||im->cols!=(im->phase?3:2))return CC_BAD;
 byte bit=1<<id;
 if(c->ids&bit)return CC_FULL;
 c->ids|=bit;
 if(x < -16||x>=160||y < -16||y>=200)return CC_OK;
 int xb=(int)((byte)(x+16)>>2)-4,xe=xb+im->cols,ye=y+im->height;
 if(xe<=0||ye<=0)return CC_OK;
 byte sx=0,sy=0;
 if(xb<0){sx=(byte)-xb;xb=0;}if(y<0){sy=(byte)-y;y=0;}
 if(xe>40)xe=40;if(ye>200)ye=200;
 byte row=(byte)y>>3,nrows=((byte)(ye-1)>>3)-row+1,w=(byte)(xe-xb),top=(byte)y&7;
 byte slot=CC_STATIC_LIMIT+c->back*CC_DYNAMIC_CODES+id*3+sx;
 volatile byte *first=c->screens[c->back]+cc_rows[row]+(byte)xb;
 CCHistory *hist=c->history+c->back;byte count=hist->count;
 if((byte)(count+w*nrows)>CC_HISTORY_ENTRIES)return CC_FULL;
 const byte *src=im->data+(word)sx*32+7+sy-top,*mask;
 // Select the native draw mode while reading the original mask pointer.
 // Rechecking the mask later can miscompile when the source offset is zero.
 if(im->mask) {
  mask=im->mask+(word)sx*32+7+sy-top;
#if CC_ASM
  c33_or=0;
#endif
 } else {
  mask=nullptr;
#if CC_ASM
  c33_or=1;
#endif
 }
 byte *lut=c->glyphs;CCEntry *entries=hist->entries;
#if CC_ASM
 c33_cell_col=first;c33_src_col=src;c33_mask_col=mask;c33_hist=(byte *)(entries+count);c33_lut=lut;
 c33_row0=row;c33_rows=nrows;c33_width=w;c33_slot=slot;c33_count=count;c33_off0=7+sy-top;c33_scol=sx;c33_bounds=(byte *)im->lo;
 __asm { jsr cc_walk_native }
 count=c33_count;
#else
 for(byte col=0;col<w;col++) {
  volatile byte *cell=first+col;const byte *sp=src+((word)col<<5),*mp=mask?mask+((word)col<<5):nullptr;
  for(byte r=0;r<nrows;r++) {
   byte off=7+sy-top+r*8;
   if(off+7<im->lo[sx+col]||off>im->hi[sx+col]){sp+=8;if(mp)mp+=8;cell+=40;continue;}
   byte old=*cell,code=slot+col,bank=((row+r)&3)<<2;
   if(old<CC_STATIC_LIMIT){CCEntry *e=entries+count++;e->cell=cell;e->old=old;*cell=code;}else code=old;
   volatile byte *dst=(volatile byte *)((word)(lut[128+code]+bank)<<8|lut[code]);
   const volatile byte *bg=(const volatile byte *)((word)(lut[128+old]+bank)<<8|lut[old]);
   if(!old)for(byte j=0;j<8;j++)dst[j]=sp[j];
   else if(!im->mask)for(byte j=0;j<8;j++)dst[j]=bg[j]|sp[j];
   else for(byte j=0;j<8;j++)dst[j]=(bg[j]&mp[j])|sp[j];
   sp+=8;if(mp)mp+=8;cell+=40;
  }
 }
#endif
 hist->count=count;if(c->poll)c->poll();return CC_OK;
}
