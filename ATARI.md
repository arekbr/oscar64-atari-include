# Atari XL/XE library reference

Run the build commands from the repository root. Use the compiler from
the [setup instructions](README.md#building-from-source) on PATH, or
substitute its full path for `oscar64`.

## Atari XL/XE headers

Run the XEX examples with BASIC disabled. In Atari800, use `-nobasic`.
Examples that return from `main` leave their output on screen. Press RESET
to leave the example.

Use `<atari/antic.h>`, `<atari/gtia.h>`, `<atari/pia.h>`, and `<atari/pokey.h>`
for direct hardware access. Use `<atari/os.h>` for OS variables and register
shadows. For GTIA and POKEY, use `gtia` and `pokey` for writes and `gtiar` and
`pokeyr` for reads. The same address can select a different register on read
and write.

With the OS vertical blank handler active, write to a shadow variable such as
`os_color[4]` to keep a color across frames. Write to `gtia.colbk` for an
immediate change. The example shows both on the same screen.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=raster.xex samples/atari/raster.c
```

Load the XEX on an Atari XL/XE in PAL mode. The upper background is red and
the lower background is green. The example relies on the standard OS vertical
blank handler.

## Atari XL/XE console and file I/O

Use `<atari/cio.h>` to open devices and read or write bytes and records.
The OS opens the screen editor E: on channel 0. Use `<atari/console.h>`
for line input, formatted output and screen controls. `<atari/fmt.h>`
also provides formatting to a buffer or another open CIO channel.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=cioread.xex samples/atari/cio_read.c
```

Boot with DOS, put `MESSAGE.TXT` on the first disk drive and load
`CIOREAD.XEX`. Use ATASCII text with `$9B` line endings. The example
reads `D:MESSAGE.TXT` and writes it to E:.

## Atari XL/XE disk file operations

Use `cio_remove()`, `cio_lock()` and `cio_unlock()` from `<atari/cio.h>`
with a device-qualified file name, such as `D1:REPORT.TXT`.
`cio_rename("D1:REPORT.TXT", "BACKUP.TXT")` renames the file in its
current directory. Each function uses a free IOCB and returns the CIO
status. File names, wildcard matching and write protection follow the
disk handler.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=ciofiles.xex samples/atari/cio_files.c
```

Run the sample under DOS on a writable scratch disk. It creates a
temporary file, locks, unlocks and renames it, then removes it.

## Atari XL/XE disk directories

Use `cio_mkdir()` and `cio_chdir()` from `<atari/cio.h>` with a
device-qualified path and `CIO_DOS_MYDOS` or `CIO_DOS_SPARTA`.
MyDOS changes the default directory for `D:` paths. SpartaDOS changes
the current directory of the named drive. Use `cio_rmdir()` to remove
an empty directory on a SpartaDOS disk. With MyDOS, pass a known
empty directory to `cio_remove()`.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=ciodirs.xex samples/atari/cio_directories.c
oscar64 -i=include/ -tm=atari -O2 -dSPARTA -o=ciodirs.xex samples/atari/cio_directories.c
```

Run the first build under MyDOS with a scratch disk in drive 1, or
the second under SpartaDOS with a SpartaDOS-formatted scratch disk
in drive 2. Start at the root. The sample creates, enters and removes
the EXAMPLE directory. It starts at `$3000` to leave room for DOS.

## Atari XL/XE byte positions under SpartaDOS X

Use `cio_file_tell()` and `cio_file_length()` from `<atari/cio.h>`
to read a 24-bit position or length in a `dword`. With
`cio_file_seek()`, supply a signed offset from `CIO_SEEK_SET`,
`CIO_SEEK_CUR` or `CIO_SEEK_END`. The handler decides whether a
position beyond EOF is valid. On AtariDOS-formatted disks, SpartaDOS X
can report a sector-rounded length. An end-relative seek uses that
reported length, and the handler may stop the seek at the actual EOF.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=ciopos.xex samples/atari/cio_file_position.c
```

Load the sample under SpartaDOS X 4.x. Place SEEK.DAT at the root of
a SpartaDOS-formatted disk in drive 2. The sample prints the file
length and the last byte, or reports an empty file.

## Atari XL/XE serial disk I/O

Use `<atari/sio.h>` to send commands through the OS serial I/O routine.
The basic sector helpers transfer 128 bytes. To handle double-density
media, query the drive with `sio_drive_status()` and use
`sio_disk_sector_size()`. The first three sectors still use 128 bytes.
Pass the drive status and buffer size to `sio_disk_read()`,
`sio_disk_write()` or `sio_disk_put()`.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=sioread.xex samples/atari/sio_read.c
```

Load `SIOREAD.XEX` with a 720-sector disk in drive 1. The example queries
the drive, then reads and displays the first 16 bytes of sector 720 when
the drive reports 128-byte sectors.

## Atari XL/XE return to DOS

Call `dos_exit()` from `<atari/dos.h>` in a program started after DOS
boots. Close files and restore any OS state changed by the program.
The helper enters DOS through its resident vector.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=dosexit.xex samples/atari/dos_exit.c
```

Load the sample from DOS with BASIC disabled. Press a key to return.

SpartaDOS X programs can call `dos_args()` before other command-line
operations to read the launch line into a caller-owned `DOSArgs`.
The program name is the first token. `dos_default_drive()` reports
the current DOS drive, and `dos_getcwd()` reads its current
directory or that of a drive specified by number. The root directory
is represented by an empty string. Check the returned CIO status
before using the path.

The context sample shows the default drive, directory, program name,
and argument tokens. Build it and put `CONTEXT.XEX` on `D1:`.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=CONTEXT.XEX samples/atari/dos_context.c
```

At the SpartaDOS X prompt, select another mounted drive and load the
sample explicitly from `D1:`.

```text
D2:
X D1:CONTEXT.XEX ALPHA D3:FOO
```

The sample shows `D2:` as the default drive. Press a key to return to DOS.

## Atari XL/XE display lists, interrupts and input

Use `<atari/dlist.h>` to build and inspect ANTIC display lists.
`<atari/irq.h>` installs display list and deferred vertical blank handlers.
Use `<atari/input.h>` to read the joystick, paddles, or keyboard.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=rasterio.xex samples/atari/raster_input.c
```

Load the XEX in PAL mode. A display list interrupt changes the background
from red to green. A vertical blank handler reads the first joystick.
Hold the fire button to turn the lower band white.

## Atari XL/XE packed bitmaps

Use `<atari/bitmap.h>` to draw on a caller-owned buffer in ANTIC modes
8 through F. Use `ABM_GTIA` with ANTIC F for four-bit pixels
and select GTIA mode 9, 10 or 11 in PRIOR.

Query the layout with `abm_mode_info()` and bind the buffer with
`abm_init()` for a normal-width playfield. Use
`abm_mode_info_width()` and `abm_init_width()` for a narrow or wide
playfield. You can read and write pixels, draw lines, clear the bitmap
and fill rectangles. The drawing functions preserve row padding.
`abm_display_list()` builds a list in a separate buffer and reloads
the screen address when a row starts at a new 4 KB page or follows
padding. Keep each row within one 4 KB page and the list within one
1 KB page. Select the matching playfield width in DMACTL and install
the list with `dl_set()`.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=bitmap.xex samples/atari/bitmap.c
oscar64 -i=include/ -tm=atari -O2 -dBITMAP_HIRES -o=bitmapf.xex samples/atari/bitmap.c
oscar64 -i=include/ -tm=atari -O2 -o=bitmapd.xex samples/atari/bitmap_modes.c
```

The first sample sets up a two-part display in ANTIC E or F.
The second draws two rectangles in ANTIC D. Build the second with
`-dBITMAP_MODE=8` through `15` to select an ANTIC mode. For a GTIA
bitmap, use `-dBITMAP_MODE=16` and `-dBITMAP_GTIA_MODE=9`, `10` or `11`.
Set `-dBITMAP_PLAYFIELD=DMACTL_PF_NARROW` or
`-dBITMAP_PLAYFIELD=DMACTL_PF_WIDE` to change the playfield width.

## Atari XL/XE bitmap drawing

Use `abm_ellipse()` and `abm_fill_ellipse()` for clipped outlines
and solid shapes. Supply a center and radii in logical pixels.
Use `abm_flood_fill()` to replace a four-connected color region.
Reserve `abm_flood_size()` bytes of caller-owned scratch space
separate from the bitmap and its view.

`abm_text()` draws transparent eight-by-eight ATASCII glyphs from
a 128-character font in screen-code order. With the XL/XE OS ROM
mapped in, the standard font begins at `$E000`. The same call accepts
a font in RAM.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=drawing.xex samples/atari/bitmap_drawing.c
```

The sample displays hollow, solid and flood-filled ellipses with
text drawn over a colored rectangle in ANTIC D.

Use `abm_copy_rect()` to copy a clipped rectangle between bound
bitmaps of the same pixel depth. A copy within one bitmap supports
overlapping rectangles. For smaller packed images, use `abm_blit()`
with an `ABitmapImage` carrying the source dimensions, byte pitch,
pixel depth and buffer size. Supply a color index for transparency
or `ABM_OPAQUE` to write every pixel.

Use `abm_polygon()` for clipped outlines or `abm_fill_polygon()`
to draw the outline and fill the interior by the even-odd rule.
Pass an array of signed `ABitmapPoint` coordinates.

The polygon fill keeps a four-row crossing mask on the stack.
The moving scene sample fills a background polygon once. It restores
the old image rectangle and draws a packed transparent image on the
other display buffer before updating the OS display-list shadow.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=scene.xex samples/atari/bitmap_scene.c
```

Use `<atari/bitmap_canvas.h>` to draw within a clip rectangle on a bound
bitmap. The canvas supports patterned and thick lines, patterned fills,
elliptical arcs, filled sectors and transparent ATASCII text. Font widths
are optional for proportional text. Arc angles increase clockwise from
positive X with 256 units per revolution. A complete sector uses a sweep
of 256.

The canvas clip also bounds polygon outlines and fills, rectangle copies
and color-key bitmap drawing. A clipped transfer preserves the original
source-to-destination alignment.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=canvas.xex samples/atari/bitmap_canvas.c
```

Use `<atari/bitmap_screen.h>` to bind two matching display pages and build
their display lists. Supply separate storage for both bitmaps and both
lists. After `abm_screen_show()`, draw to `abm_screen_back()` and call
`abm_screen_present()` to publish it. The OS vertical blank handler must
be enabled. The present call waits for the new list to be displayed
before the old front page can be reused.

Use `abm_page_init()` and `abm_page_show()` to select independently prepared
pages with their own ANTIC modes and playfield widths. Draw through the
`bitmap` field of any prepared page. See `samples/atari/bitmap_independent_pages.c`.

Align both lists to 256 bytes for single-byte publication through the OS
shadow and one swap per frame. The full-screen sample selects GRAPHICS 8,
9, 10 or 15 through `DISPLAY_MODE`. Each 192-line page crosses a 4 KB
boundary between rows, with an LMS at the new boundary.

```sh
oscar64 -i=include/ -tm=atari -O2 -dDISPLAY_MODE=15 -o=pages.xex samples/atari/bitmap_pages.c
```

Use `<atari/bitmap_image.h>` for four-color MIC screen dumps and Koala
MicroIllustrator PIC files. Decode packed bytes already in memory or load
them through CIO with caller-provided file scratch. PIC supports stored
rows, row-order RLE and even/odd column-order RLE. The functions return
palette bytes without altering OS color shadows. The image sample reads
`D:DEMO.PIC`. Compile with `-dDEMO_MIC` to read `D:DEMO.MIC`.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=picture.xex samples/atari/bitmap_image.c
```

## Atari XL/XE OS graphics

Use `<atari/graphics.h>` to open the OS display handler S: in GRAPHICS
mode 0 through 15. Query the dimensions with `gr_mode_info()`. Pass
`GR_SPLIT` to leave a four-line E: text window in modes 1–8 and 12–15.
Keep E: open on channel 0 for split screens.

`gr_plot()` writes one pixel or character. Read it back with
`gr_locate()`. In bitmap modes, `gr_draw_to()` starts at the previous
plot or line endpoint. `gr_clear()` clears the screen and text window.
`gr_close()` releases the channel and leaves the image on screen.

The OS allocates its display memory at the top of RAM. Place the
program, heap and stack below that area, as shown in the sample.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=screen.xex samples/atari/screen_graphics.c
```

The sample draws a square and diagonal in GRAPHICS 7 and writes
a caption in the text window.

## Atari XL/XE player and missile graphics

Use `<atari/pmg.h>` with an aligned player/missile buffer and
`<atari/colors.h>` for named colors. The sample draws player 0 with
single-line DMA. Define `PMG_DOUBLE_LINES` to use double-line DMA.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=pmg.xex samples/atari/pmg.c
```

## Atari XL/XE POKEY audio

Use `<atari/sound.h>` to set the frequency, distortion and volume of a
POKEY channel. The sample plays a short tone and silences all four
channels.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=beep.xex samples/atari/pokey_beep.c
```

## Atari XL/XE extended memory on the 130XE

Use `<atari/xmem.h>` to detect and transfer data to and from the extra
banks. Place the `xmemcode` section outside the bank window as shown. The
example probes for extra banks and verifies a transfer.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=xmem.xex samples/atari/xmem.c
```

## Atari XL/XE POKEY timer interrupts

Use `<atari/ptimer.h>` to attach a callback to hardware timer 1, 2 or 4.
The example counts timer 1 interrupts during 30 PAL frames.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=potimer.xex samples/atari/pokey_timer.c
```

## Atari XL/XE text windows

Use `<atari/charwin.h>` to display text in rectangular views of screen
memory. The example places a status field alongside a scrolling message
window.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=txtwins.xex samples/atari/text_windows.c
```

## Atari XL/XE display list interrupt chains

Use `<atari/dlic.h>` to dispatch a sequence of display list interrupts.
The sample changes the background color at three positions in the display.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=dlichain.xex samples/atari/dli_chain.c
```

## Atari XL/XE virtual players

Use `<atari/vpmg.h>` to reuse the four hardware player positions in
different vertical zones. The example shows eight players in two bands.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=vplayers.xex samples/atari/virtual_players.c
```

## Atari XL/XE POKEY sound envelopes

Use `<atari/senv.h>` to set attack, decay, sustain and release on a POKEY
channel. The sample plays one note with a gradual attack and release.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=envnote.xex samples/atari/sound_envelope.c
```

## Atari XL/XE RAM under the OS ROM

Use `<atari/romram.h>` to access RAM underneath the OS ROM while keeping
the system interrupt handlers available. The sample verifies and restores
one byte, then closes the window before using console I/O.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=romram.xex samples/atari/romram.c
```

## Atari XL/XE ANTIC 2 glyph cache

Use `<atari/gscreen.h>` to map logical cells to character slots in a
normal-width ANTIC 2 display. The sample uses an immutable glyph atlas and
a caller-owned code buffer.

```sh
oscar64 -i=include/ -tm=atari -O2 -dGS_CODES=codes -dGS_W=8 -dGS_H=4 -o=glyphs.xex samples/atari/glyph_screen.c
```

## Atari XL/XE POKEY note sequences

Use `<atari/pseq.h>` with an immutable bank of note, instrument and track
data. The sample advances a two-voice sequence once per PAL frame.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=pokeyseq.xex samples/atari/pokey_sequence.c
```

## Atari XL/XE ANTIC 4 character compositing

Use `<atari/cscache.h>` for character compositing with caller-owned
display and font buffers. `<atari/ccmono.h>` adds a one-color image
adapter.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=monocmp.xex samples/atari/mono_compositor.c
```

## Atari XL/XE buffered player and missile graphics

Use `<atari/pmbatch.h>` to prepare player/missile data for presentation.
`<atari/pmdirect.h>` adds an optional direct path for drawing into the
inactive buffer.

```sh
oscar64 -i=include/ -tm=atari -O2 -o=pmgswap.xex samples/atari/pmg_swap.c
```

## Atari XL/XE low RAM after DOS loading

For standalone XL/XE programs with 64 KB, `<atari/lowram.h>` can claim low
RAM after the DOS loader has transferred control. Follow the startup
requirements in the header. After takeover, use cold RESET to exit.

## Atari XL/XE convenience header

`<atari/atari.h>` collects the hardware and OS-backed display, input and
console helpers. Include individual headers to use a smaller group of
interfaces.

## Additional graphics and display features

Use `<atari/bitmap_indexed.h>` for indexed BMP, PCX and GIF. Decode from
memory or read sequentially from CIO. The bitmap uses the low bits of
each source index or a caller-supplied 256-entry color map. A CIO load
uses 256 bytes of input scratch. GIF needs an additional 16 KiB LZW work
area and draws the first frame. The decoder honors GIF interlace and
transparency. Decode to an off-screen destination when an error must
leave the old image intact.

Build `samples/atari/bitmap_indexed.c` with `-dDEMO_BMP`, `-dDEMO_PCX`
or `-dDEMO_GIF` and place the corresponding DEMO file on D:.

The canvas also draws a filled and outlined three-dimensional bar with
`abm_canvas_bar_3d()`. Use `<atari/bitmap_stroke_font.h>` to bind or
load a BGI CHR font, measure it and draw scaled strokes using the canvas
pen and clip.

`<atari/bitmap_font.h>` loads 1 KiB Atari FNT files and prepares
proportional 8-by-8 glyphs. Supply 128 bytes for the measured widths.

`<atari/pmg.h>` can move player and missile shapes and erase their old
positions. `<atari/char_sprite.h>` draws transparent 2-bit sprites on an
ANTIC 4 or 5 map using caller-supplied charset pages and a backdrop.

`<atari/pmg_cursor.h>` provides a movable cursor on one reserved player,
with a caller-supplied shape, hot spot and movement bounds.
`pmg_cursor_step()` accepts `joy_read()` directions. See
`samples/atari/pmg_cursor.c`.

`<atari/graphics_plus.h>` builds the 0++, 4++ and 10++ display lists.
`<atari/graphics_text40.h>` displays 40 columns in six-scanline ANTIC 2
rows. Both drivers use display list interrupts. The 40-row mode also
uses the deferred vertical blank.

`<atari/graphics_text80.h>` draws 80 columns by 24 rows on a 320x192
ANTIC F bitmap on a stock XL/XE. Supply 128 eight-byte glyphs in Atari
screen-code order, with bits 3 through 0 from left to right in each byte.
`gr_text80_put()` and `gr_text80_write()` accept ATASCII with bit 7 for
inverse video. `gr_text80_scroll()` moves the image up by character rows.
The caller owns the bitmap, display list and colors. See
`samples/atari/graphics_text80.c` for a display list and an example font.

`<atari/hcm.h>` provides a 28-by-25 PMG color map over an ANTIC E or F
bitmap. `hcm_show_rows()` can schedule a GTIA color register for each
of the 25 rows. It uses 1172 bytes of caller-owned executable RAM.
See `samples/atari/hcm_rows.c`. Its raster handler runs for the visible 200 scanlines.
`hcm_read_scanline()` reads the logical color index at any of those
scanlines, including the columns drawn with missiles. See
`samples/atari/hcm_scanline.c`.

`<atari/vbxe.h>` detects an optional VideoBoard XE FX and transfers data
through its banked 4 KiB RAM window. It provides RGB palettes, raw XDL
and blitter control, and a 320-pixel-wide indexed overlay.

Use `<atari/vbxe_image.h>` to load indexed BMP, PCX and GIF files into
an 8-bit VBXE surface up to 336 by 256 pixels. `vbxe_load_image()`
streams through CIO and `vbxe_decode_image()` decodes from memory. Both
preserve the file indices and program the active file color table after
a successful decode. Loading GIF uses the first frame and preserves the
destination indices at transparent pixels.

Supply `VBXE_IMAGE_WORK` bytes of scratch plus `ABM_INDEXED_CIO_WORK`
for CIO and `ABM_INDEXED_GIF_WORK` for GIF. Keep code, stack, scratch and
descriptors outside the MEMAC-A window at $A000–$AFFF. Errors can leave
previously decoded pixels in VRAM. Use a spare surface and palette bank
when preserving the active picture during a load.

Build `samples/atari/vbxe_image.c` with `-dDEMO_BMP`, `-dDEMO_PCX`
or `-dDEMO_GIF`. It loads `DEMO.BMP`, `DEMO.PCX` or `DEMO.GIF`
from D: and displays the result in a 320 by 192 overlay.

`<atari/vbxe_display.h>` selects narrow, normal and wide overlays in
low-resolution indexed, standard indexed, packed high-resolution or
hardware text mode. Its graphics calls draw pixels,
lines and filled rectangles. Text calls read and write glyph and
attribute pairs within a row. Use standard mode with wide width to
display a full 336-pixel image. The caller owns the XDL, pixel and
charset allocations.

Use `<atari/vbxe_terminal.h>` to stream ASCII and ANSI controls to the
hardware text layer. It supports cursor movement, colors, erasure
and scrolling. `samples/atari/vbxe_terminal.c` shows setup and output.
Use `vbxe_terminal_scrollback_bind()` with one caller-owned text row to
keep the first outgoing row and its attributes before an ANSI or direct
scroll. Acknowledge it with `vbxe_terminal_scrollback_take()`.
See `samples/atari/vbxe_scrollback.c`. Use `-dDEMO_ROWS=32` to try the
32-row display.

`vbxe_save_bmp()` in `<atari/vbxe_image.h>` saves an 8-bit VRAM surface
through CIO with a caller-supplied RGB table.
`samples/atari/vbxe_save_bmp.c` writes an odd-width BMP.

`vbxe_display_show_map()` programs an attribute map for a VBXE display.
`vbxe_display_copy()` uses the blitter for overlapping copies within a
bitmap or text surface. See `samples/atari/vbxe_operations.c`.

`<atari/char_sprite_animation.h>` adds timed animation clips and per-tick
movement to the ANTIC 4/5 software sprite compositor. See
`samples/atari/char_sprite_animation.c` for double-buffered rendering.

`<atari/char_sprite_video.h>` presents a sprite screen with automatic
glyph-bank changes at band boundaries. The mode builds the ANTIC 4/5
list and resets the first bank in the immediate VBI. See
`samples/atari/char_sprite_bands.c`.

`char_sprite_pmg_render_rotating()` gives actor 0 priority and retries
the first actor skipped for lack of a player pair on the next frame.
Keep the cursor in a separate byte initialized to zero. The frame can
be displayed when the result is `CHAR_SPRITE_FULL`. See
`samples/atari/char_sprite_rotation.c`. Keep the video descriptor in
writable RAM when using `-O3` with the compilers tested for this example.

## Atari graphics compatibility

* [CIO device 80:](samples/atari/cio80_device.c) exposes a normal
  ANTIC F bitmap as 80 columns by 24 rows on stock XL/XE. The handler
  uses a caller-owned 4×8 font and returns the display to its previous
  state on close.
* [CIO S2:](samples/atari/s2_cio.c) verifies the resident `S_VBXE.SYS`
  BIOS with XIO 96 and opens its VBXE graphics and text modes. Install
  the original BIOS first and reserve the memory below DOS MEMLO.
* [PMG pointing devices](samples/atari/pmg_pointer.c) support the Atari
  ST and Amiga mice, Atari Trak-Ball and Touch Tablet. Poll relative
  devices between frames and apply the resulting motion in the
  foreground once per frame.
* [POKEY pointer sampling](samples/atari/pointer_irq.c) samples Atari ST,
  Amiga and Trak-Ball devices through timer 2 at about 1 kHz. The
  foreground reads the movement and trigger with `pointer_irq_take()`.
  Keep channel 2 available and use the standalone 64 kHz clock. The
  sampler pauses in OS critical sections and resumes on the next
  foreground read. Call `pointer_irq_stop()` to restore the old IRQ
  vector and the caller-supplied AUDF2/AUDC2 settings.
  `<atari/gr0_cursor.h>` inverts one character on the standard OS
  GRAPHICS 0 screen. Hide both the mouse cursor and the OS editor caret
  before writing to the screen or changing modes.
* [Dynamic MOU drivers](samples/atari/mou_driver.c) load the original
  Atari and Atari XL cc65 mouse ABI 6 files through CIO. Reserve POKEY
  channel and timer 2 and provide their previous settings on open.
  Call `amou_poll()` after each vertical blank, read `amou_info()`
  and render the cursor in your application. The drivers read the
  first joystick port. Both Atari driver variants can be called with
  the OS ROM mapped. Use a trusted module and keep the relocated
  driver image in ordinary RAM.
* [Dynamic TGI drivers](samples/atari/tgi_driver.c) load cc65 Atari
  and Atari XL small-model o65 modules through CIO. Give the loader
  separate file and image buffers and reserve live program memory
  before activating the driver.
* [The Atari TGI source interface](samples/atari/tgi.c) provides
  `<tgi.h>` for use with Atari and Atari XL ABI 6 graphics modules.
  It adds the graphics cursor, clipping, geometric shapes, bitmap
  text and `.TCH` stroke fonts. The default file is `D:ATR8.TGI`.
  Names without a device use `D:`.

  Build the sample with
  `oscar64 -i=include/ -tm=atari -O2 -dTGI_PROGRAM_END=0x7e00 samples/atari/tgi.c`.
  Put `ATR8.TGI` on the DOS disk. For an embedded driver, put the
  same file next to the sample and add `-dTGI_EMBED_DRIVER`. The
  application declares `tgi_static_stddrv` as an `ATgiModule`
  descriptor for the embedded file. cc65's prelinked driver objects
  depend on the cc65 runtime.

  Set `TGI_PROGRAM_END` to the upper exclusive address of live
  program memory, including the Oscar64 static call frames, stack
  and heap. Keep the program below the screen buffers using a
  matching `#pragma region`. Without the flag, TGI uses the default
  Atari program boundary of `$BC00`, which leaves too little room
  for these drivers. Dynamic loading temporarily needs about 4.8
  KiB of free heap. Uninstalling releases the relocated image.
  Only one TGI driver may be attached at a time.
