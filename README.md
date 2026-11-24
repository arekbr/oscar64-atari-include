# Oscar64 Atari XL/XE library

C headers, library routines, and sample programs for the Atari 800XL
and 65XE with 64 KiB of RAM. This library extends the existing
`-tm=atari` target in [Oscar64](https://github.com/drmortalwombat/oscar64).
The examples use ANTIC, GTIA, POKEY, and the Atari OS. Selected examples
cover 130XE banked RAM, DOS, or optional hardware such as VideoBoard XE.

- [Sample catalog](samples/atari/README.md) pairs the Atari examples with
  their counterparts in the standard Oscar64 C64 catalog.
- [Library reference](ATARI.md) covers the headers, APIs, and build options.
- [OscarTutorials for Atari XL/XE](https://github.com/arekbr/OscarTutorials-Atari-XLXE)
  contains the separate numbered lessons.

## Building from source

The tested toolchain is
[Oscar64 4f6df8b](https://github.com/drmortalwombat/oscar64/tree/4f6df8bd137d55c553673b38d181b5ae1fdf09a6).
On Linux, building it requires Git, GNU Make, CMake 3.15 or later, a C++17
compiler, and the X11 development headers. Run these commands from the root
of this repository.

```sh
git clone https://github.com/drmortalwombat/oscar64.git build/oscar64-source
git -C build/oscar64-source checkout --detach 4f6df8bd137d55c553673b38d181b5ae1fdf09a6
cmake -S build/oscar64-source -B build/oscar64-host -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/build/toolchain"
cmake --build build/oscar64-host
cmake --install build/oscar64-host
make -C samples atari OSCAR64_CC="$PWD/build/toolchain/bin/oscar64"
make -C samples library OSCAR64_CC="$PWD/build/toolchain/bin/oscar64"
```

The first Make target builds 64 XEX files and the second builds 53 library
examples. The output directory is `samples/atari/build/`. With the tested
compiler already on PATH, you can omit `OSCAR64_CC`.

Build a single application with the library include path.

```sh
build/toolchain/bin/oscar64 -i="$PWD/include/" -tm=atari -O2 -o=lines.xex samples/atari/hires/lines.c
```

Run XEX files with BASIC disabled. In Atari800, a simple first test is:

```sh
atari800 -xl -pal -nobasic -run samples/atari/build/stdio/helloworld.xex
```

## Attribution and license

Oscar64 and the original C64 examples and resources are by
[drmortalwombat and contributors](https://github.com/drmortalwombat/oscar64).
This repository contains the Atari adaptations and library additions.
See [LICENSE](LICENSE) for the GNU General Public License v3.0.
