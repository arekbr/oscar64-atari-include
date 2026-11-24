# Atari XL/XE examples

The subdirectories follow the categories in the standard Oscar64 sample
catalog. The table pairs 62 Atari ports with their C64 originals and
explains the six C64 cartridge examples. Compare the sources side by
side to see how the same exercise uses each machine's hardware. Graphics
programs use ANTIC, GTIA and Atari screen memory. Disk examples use CIO.
The graphics and game exercises use hardware and controls appropriate for
an Atari XL/XE with 64 KiB RAM. Run with BASIC disabled. The standalone
[romram.c](romram.c) example verifies a single byte under OS ROM and restores it. The
[xmem.c](xmem.c) bank-window exercise needs a 130XE or equivalent expansion.

## Building and running

Follow the [toolchain setup](../../README.md#building-from-source) and run
these commands from the repository root.

```sh
make -C samples atari OSCAR64_CC="$PWD/build/toolchain/bin/oscar64"
atari800 -xl -pal -nobasic -run samples/atari/build/stdio/helloworld.xex
```

The catalog builds 64 XEX files, including ROMRAM and XMEM. Set
`OUT=/absolute/path/to/output` to select another output directory.
Run `make -C samples clean` with the same `OUT` before rebuilding with a
different compiler. The [library reference](../../ATARI.md) describes
the separate library examples and their hardware requirements.

The makefile sets `-O1` for `hires/bitblit.c`, `hires/cube3d.c`,
`hiresmc/paint.c` and `kernalio/hireswrite.c` for compiler compatibility. Build
`hiresmc/paint.c` with `-O1 -dPAINT_MOUSE` to use an Atari ST mouse on
port 1. Atari800 can map its numeric keypad to joystick 1.
START exits interactive demos that show or document that control. Use RESET
to leave fullscreen examples that keep their display active.

## Examples that use DOS

Boot a writable Atari DOS 2-style disk in D1 and load the XEX from DOS.
MyDOS 4.53 is used for the disk exercises. The write examples ask for a
scratch disk if their destination already exists.

| First program or fixture | Follow-up |
| --- | --- |
| `kernalio/charwrite.xex` creates `CHARS.DAT` | `kernalio/charread.xex` prints each byte. |
| `kernalio/filewrite.xex` creates `SCORE.DAT` | `kernalio/fileread.xex` displays four records. |
| `kernalio/hireswrite.xex` creates `SAMPLE.MIC` | `kernalio/hiresread.xex` displays the packed 320×200 smiling face. |
| Copy [blumba2.rle](kernalio/blumba2.rle) as `BLUMBA2.RLE` | `kernalio/hiresfload.xex` decompresses and displays it. |
| Copy [tiles.fnt](memmap/tiles.fnt) as `TILES.FNT` | `memmap/charsetload.xex` loads the 1024-byte font. |
| Copy [ovl1.bin](memmap/ovl1.bin) through [ovl6.bin](memmap/ovl6.bin) as `OVL1.BIN` through `OVL6.BIN` | `memmap/overlay.xex` loads and calls all six modules at `$8000`. |

`kernalio/diskdir.xex` lists a DOS directory. `memmap/overlaylzo.xex`
demonstrates the corresponding compressed, embedded overlay without DOS.

## Verification

The 64 catalog builds were checked in Atari800 with the 800XL rev. 2
OS ROM in PAL. The disk examples used MyDOS, and XMEM used the 130XE
configuration. Selected examples were also checked in NTSC.
Testing on a physical Atari is pending.

## Hardware differences

The ports retain the source data, movement and scene composition. Atari's
playfield has fewer simultaneous colors in several modes, so some shades
use a mixed palette. GTIA 10 has 80 color pixels per scan line. The sprite
tests use it to retain overlapping actor counts beyond the four hardware
players. An ANTIC text page holds 128 glyphs and the hardware inverse bit.
The 1541 fast-loader and C64 cartridge mapper examples use machine-specific
protocols. CIO disk access and 130XE banking are covered by separate code.

## Standard sample equivalents

### stdio

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [helloworld.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/stdio/helloworld.c) | [helloworld.c](stdio/helloworld.c) | Print a line with standard C I/O. Atari supplies the ATASCII newline. |

### fractals

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [mbfixed.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbfixed.c) | [mbfixed.c](fractals/mbfixed.c) | Render the original 160×100 4.12 fixed-point Mandelbrot with its paired 200-line dither. |
| [mbhires.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbhires.c) | [mbhires.c](fractals/mbhires.c) | Render the original 320×200 floating-point monochrome Mandelbrot. |
| [mbmulti.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbmulti.c) | [mbmulti.c](fractals/mbmulti.c) | Render the original floating-point Mandelbrot with eight paired-scanline color patterns. |
| [mbmulti3d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbmulti3d.c) | [mbmulti3d.c](fractals/mbmulti3d.c) | Project the original smoothed escape counts with the same view, lighting and vertical strokes. |
| [mbtext.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbtext.c) | [mbtext.c](fractals/mbtext.c) | Render the original 40×25 escape-count grid using GTIA mode 10 and paired color cells. |
| [mbzoom.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/fractals/mbzoom.c) | [mbzoom.c](fractals/mbzoom.c) | Use the original fixed-point square lookup and color lookup. WASD pans, plus and minus zoom, and ESC exits. |

### hires

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [bitblit.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/bitblit.c) | [bitblit.c](hires/bitblit.c) | Draw the source stencil and compare all eleven Boolean, copy and pattern operations. |
| [cube3d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/cube3d.c) | [cube3d.c](hires/cube3d.c) | Project the eight original 4.12 vertices and animate one hundred wireframe poses with XOR lines. |
| [fractaltree.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/fractaltree.c) | [fractaltree.c](hires/fractaltree.c) | Draw the original recursive tree on a 320×200 bitmap. |
| [func3d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/func3d.c) | [func3d.c](hires/func3d.c) | Project and depth-sort the original 30×30 mesh. Shade and outline each face. |
| [lines.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/lines.c) | [lines.c](hires/lines.c) | Draw the original 320×200 line fan and apply the same sequence of set and clear patterns. |
| [polygon.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/polygon.c) | [polygon.c](hires/polygon.c) | Draw 128 rotating and growing star polygons with the original gray patterns and clip rectangle. |
| [qsort.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/qsort.c) | [qsort.c](hires/qsort.c) | Show the original 160-value quicksort as vertical bars and report the elapsed time in seconds. |
| [splitscreen.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hires/splitscreen.c) | [splitscreen.c](hires/splitscreen.c) | Draw the 160-line checked bitmap above the text editor. Enter X Y R to add a filled circle. Q exits. |

### hiresmc

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [floodfill.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hiresmc/floodfill.c) | [floodfill.c](hiresmc/floodfill.c) | Draw the original series of 20 solid and 20 clearing circles, then flood the same two seeds. |
| [func3d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hiresmc/func3d.c) | [func3d.c](hiresmc/func3d.c) | Project and depth-sort the original 30×30 mesh in four grayscale inks with dithered intermediate shades. |
| [paint.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hiresmc/paint.c) | [paint.c](hiresmc/paint.c) | Paint with the original cursor and ten solid or mixed inks. Joystick 1 or an ST mouse moves the cursor. F fills, C clears and START exits. |
| [polygon.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/hiresmc/polygon.c) | [polygon.c](hiresmc/polygon.c) | Draw the same 128 rotating stars with the original four-color frame and mixed fills. |

### kernalio

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [charread.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/charread.c) | [charread.c](kernalio/charread.c) | Read and print the bytes written by charwrite, observing CIO status at end of file. |
| [charwrite.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/charwrite.c) | [charwrite.c](kernalio/charwrite.c) | Write byte values 0 to 127 using one CIO character call per byte. |
| [diskdir.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/diskdir.c) | [diskdir.c](kernalio/diskdir.c) | Read DOS directory records and print names, sizes and free sectors. |
| [fileread.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/fileread.c) | [fileread.c](kernalio/fileread.c) | Read and display the array of score records written by filewrite. |
| [filewrite.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/filewrite.c) | [filewrite.c](kernalio/filewrite.c) | Write the four score records as binary data in one CIO transfer. |
| [hiresfload.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/hiresfload.c) | [hiresfload.c](kernalio/hiresfload.c) | Load and expand the original Blumba2 picture. CIO and RLE replace the 1541 fast loader and its compression stream. |
| [hiresread.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/hiresread.c) | [hiresread.c](kernalio/hiresread.c) | Read and display the same 320×200 image made by hireswrite. The file uses ANTIC scanline order. |
| [hireswrite.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/kernalio/hireswrite.c) | [hireswrite.c](kernalio/hireswrite.c) | Draw the same smiling face and write it in 200-byte chunks through CIO. |

### memmap

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [allmem.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/allmem.c) | [memmap/allmem.c](memmap/allmem.c) | Fill and verify 1 KB blocks under the OS ROM. The Atari program keeps code and its heap outside that window and remaps the ROM before printing. |
| [charsetcopy.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/charsetcopy.c) | [charsetcopy.c](memmap/charsetcopy.c) | Copy the supplied font into ANTIC-aligned RAM and show the glyph grid. |
| [charsetexpand.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/charsetexpand.c) | [charsetexpand.c](memmap/charsetexpand.c) | Expand the supplied LZO font into RAM and show the glyph grid. |
| [charsethi.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/charsethi.c) | [charsethi.c](memmap/charsethi.c) | Load the font segment at $A000 and select it with CHBAS. |
| [charsetlo.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/charsetlo.c) | [charsetlo.c](memmap/charsetlo.c) | Place the XEX font segment at $0800 and display ten Hello World lines. Use a bare XEX loader. |
| [charsetload.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/charsetload.c) | [charsetload.c](memmap/charsetload.c) | Load the supplied font through CIO. The fixture contains the 128 ANTIC glyphs. |
| [easyflash.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/easyflash.c) | C64 cartridge example | EasyFlash banked cartridge. See [PORTB RAM banking](xmem.c) for bank selection. |
| [easyflashcall.cpp](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/easyflashcall.cpp) | C64 cartridge example | C++ calls across EasyFlash ROM banks. See [CIO overlays](memmap/overlay.c) for calling a loaded entry point. |
| [easyflashlow.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/easyflashlow.c) | C64 cartridge example | Low-memory EasyFlash startup and data placement. See [low font placement](memmap/charsetlo.c) for XEX memory layout. |
| [easyflashreloc.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/easyflashreloc.c) | C64 cartridge example | Copy and execute code linked for a different address from its source bank. See [CIO overlays](memmap/overlay.c) for execution from a RAM window. |
| [easyflashshared.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/easyflashshared.c) | C64 cartridge example | Select banks from code shared across EasyFlash banks. See [PORTB RAM banking](xmem.c) for stock Atari bank access. |
| [largemem.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/largemem.c) | [largemem.c](memmap/largemem.c) | Use the heap above $A000 with BASIC disabled and report the total allocation. [XMEM](xmem.c) demonstrates a separate 130XE bank window. |
| [magicdesk.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/magicdesk.c) | C64 cartridge example | Magic Desk cartridge mapper. See [PORTB RAM banking](xmem.c) and [CIO overlays](memmap/overlay.c). |
| [overlay.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/overlay.c) | [overlay.c](memmap/overlay.c) | Load and invoke six alternate modules at the same RAM address through CIO. |
| [overlaylzo.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/overlaylzo.c) | [overlaylzo.c](memmap/overlaylzo.c) | Expand and invoke six alternate routines at the same RAM address. The LZO modules are embedded in the XEX. |
| [tsr.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/memmap/tsr.c) | [tsr.c](memmap/tsr.c) | Install a standalone Hello World routine at $8000 and return. Call it with the DOS run-at-address command while E: is open. |

### particles

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [fireworks_hires.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/particles/fireworks_hires.c) | [fireworks_hires.c](particles/fireworks_hires.c) | Use the original three launch sites, 10.6 motion and pointer-linked particle lists. |
| [fireworks_ptr.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/particles/fireworks_ptr.c) | [fireworks_ptr.c](particles/fireworks_ptr.c) | Use the original pointer-linked lists and three colored launches. ANTIC doubles the bitmap scanlines vertically. |
| [fireworks_stripe.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/particles/fireworks_stripe.c) | [fireworks_stripe.c](particles/fireworks_stripe.c) | Use the original striped 9.7 particle state and indexed linked lists. |

### rasterirq

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [autocrawler.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/rasterirq/autocrawler.c) | [autocrawler.c](rasterirq/autocrawler.c) | Scroll the original text in the bottom row from a vertical blank callback. |
| [colorbars.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/rasterirq/colorbars.c) | [colorbars.c](rasterirq/colorbars.c) | Set the same 15 successive eight-scanline color bands with display list interrupts. |
| [movingbars.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/rasterirq/movingbars.c) | [movingbars.c](rasterirq/movingbars.c) | Animate the original two 16-scanline sinusoidal raster bars and handle their overlap. |
| [openborder.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/rasterirq/openborder.c) | [openborder.c](rasterirq/openborder.c) | Move the four original bears at different rates below the text playfield. ANTIC blank lines leave player/missile DMA active. |
| [textcrawler.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/rasterirq/textcrawler.c) | [textcrawler.c](rasterirq/textcrawler.c) | Scroll the original bottom-row text from the foreground at the source pixel rate. |

### scrolling

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [bigfont.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/scrolling/bigfont.c) | [bigfont.c](scrolling/bigfont.c) | Scroll the original text in enlarged glyph columns through sixteen raster colors. |
| [cgrid8way.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/scrolling/cgrid8way.c) | [cgrid8way.c](scrolling/cgrid8way.c) | Steer the original four-color tile map with the joystick and ANTIC fine scrolling. |
| [colorram.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/scrolling/colorram.c) | [colorram.c](scrolling/colorram.c) | Scroll the original random solid and checker columns. ANTIC character pixels encode mixed colors. |
| [grid2d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/scrolling/grid2d.c) | [grid2d.c](scrolling/grid2d.c) | Scroll the original monochrome tile map on both axes with the same automatic acceleration and reflections. |
| [tunnel.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/scrolling/tunnel.c) | [tunnel.c](scrolling/tunnel.c) | Scroll the original procedurally generated corridor using the source acceleration and direction changes. |

### sprites

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [creditroll.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/sprites/creditroll.c) | [creditroll.c](sprites/creditroll.c) | Scroll the original credit text and doubled custom glyphs with ANTIC scanline addresses. |
| [joycontrol.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/sprites/joycontrol.c) | [joycontrol.c](sprites/joycontrol.c) | Move the original bear with joystick 1 using paired player/missile graphics. |
| [multiplexer.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/sprites/multiplexer.c) | [multiplexer.c](sprites/multiplexer.c) | Animate the original 16 digits on their circular paths. Use a GTIA bitmap for overlap beyond four hardware players. |
| [sprmux32.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/sprites/sprmux32.c) | [sprmux32.c](sprites/sprmux32.c) | Animate all 32 digits using the original sine and cosine tables and packed software sprites. |
| [sprmux64.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/sprites/sprmux64.c) | [sprmux64.c](sprites/sprmux64.c) | Animate 64 original ball sprites in eight rotating rows, preserving the changing draw order. |

### games

| C64 source | Atari XL/XE source | Lesson |
| --- | --- | --- |
| [breakout.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/breakout.c) | [breakout.c](games/breakout.c) | Play the original 96-brick board with three balls, joystick control and POKEY effects. Press START to exit. |
| [connectfour.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/connectfour.c) | [connectfour.c](games/connectfour.c) | Play the original seven-column, six-row game with bouncing drops, four-move opening tables and the five-ply search. |
| [hscrollshmup.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/hscrollshmup.c) | [hscrollshmup.c](games/hscrollshmup.c) | Fly over the original tile map, shoot enemy waves and accumulate lives and score. ANTIC scrolls the playfield and multiplexes enemy players. Three missiles make the ship shadow. |
| [lander.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/lander.c) | [lander.c](games/lander.c) | Fly the original ship over the generated terrain. Use the joystick for thrust and steering and reach the four-tile pad. |
| [maze3d.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/maze3d.c) | [maze3d.c](games/maze3d.c) | Explore the original 16×16 maze with gray wall glyphs, half-step movement and sliding turns. |
| [missile.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/missile.c) | [missile.c](games/missile.c) | Defend six cities with the original missile, explosion, ammo, level and scoring rules. Use the joystick and Fire. |
| [snake.c](https://github.com/drmortalwombat/oscar64/blob/4f6df8bd137d55c553673b38d181b5ae1fdf09a6/samples/games/snake.c) | [snake.c](games/snake.c) | Play on the original 40×25 board with a linked trail, random hearts, collision flash and restart. |
