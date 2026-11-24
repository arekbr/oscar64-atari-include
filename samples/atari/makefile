OSCAR64_CC ?= oscar64
OSCAR64_CFLAGS ?= -O2
OUT ?= build

# The subdirectories contain the equivalents of the standard sample series.
sources := $(wildcard */*.c) romram.c xmem.c
programs := $(sources:%.c=$(OUT)/%.xex)
library_sources := $(filter-out romram.c xmem.c,$(wildcard *.c))
library_programs := $(library_sources:%.c=$(OUT)/%.xex)
sample_headers := $(wildcard */*.h)
sample_assets := $(wildcard */*.fnt */*.bin */*.rle)
atari_library := $(wildcard ../../include/atari/*.h ../../include/atari/*.c) ../../include/tgi.h
atari_include := $(abspath ../../include)/
sample_flags = $(OSCAR64_CFLAGS)

.PHONY: all library clean
all: $(programs)

library: $(library_programs)

clean:
	@$(RM) $(foreach ext,xex asm int lbl map dbj bcs,$(patsubst %.xex,%.$(ext),$(programs) $(library_programs)))

$(OUT)/%.xex: %.c $(sample_headers) $(sample_assets) $(atari_library)
	@mkdir -p "$(@D)"
	"$(OSCAR64_CC)" "-i=$(atari_include)" -tm=atari $(sample_flags) "-o=$@" "$<"


# These examples require geometry or a program boundary at compile time.
$(OUT)/glyph_screen.xex: sample_flags += -dGS_CODES=codes -dGS_W=8 -dGS_H=4
$(OUT)/tgi.xex: sample_flags += -dTGI_PROGRAM_END=0x7e00

# The older optimizer stalls at level 2 for these drawing examples.
$(OUT)/hires/bitblit.xex $(OUT)/hires/cube3d.xex $(OUT)/hiresmc/paint.xex $(OUT)/kernalio/hireswrite.xex: sample_flags = $(filter-out -O%,$(OSCAR64_CFLAGS)) -O1

$(OUT)/games/breakout.xex: ../resources/breakoutchars.bin ../resources/breakoutsprites.bin
$(OUT)/games/lander.xex: ../resources/landersprites.bin
$(OUT)/games/maze3d.xex: ../resources/maze3dchars.bin
$(OUT)/games/connectfour.xex: ../resources/connect4chars.bin ../resources/connect4sprites.bin
$(OUT)/games/hscrollshmup.xex: ../resources/breakoutchars.bin $(wildcard ../resources/hscrollshmup*.bin)
$(OUT)/games/missile.xex: ../resources/missilechars.bin ../resources/missilesprites.bin
$(OUT)/hiresmc/paint.xex: ../resources/mouse.spd
$(OUT)/sprites/joycontrol.xex $(OUT)/rasterirq/openborder.xex: ../resources/friendlybear.bin
$(OUT)/sprites/creditroll.xex: ../resources/scifiglyph.bin
$(OUT)/sprites/multiplexer.xex $(OUT)/sprites/sprmux32.xex: ../resources/digitsprites.bin
$(OUT)/sprites/sprmux64.xex: ../resources/ballsprite.bin
