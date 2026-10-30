# Third-party components

The playable (Vulkan) build is made from the parts below, each under its own
license. A built game folder carries this file and the license texts in
`licenses/`.

## The runtime: xboxrecomp

[xboxrecomp](https://github.com/sp00nznet/xboxrecomp), in the version kept by
[ngbrecomp](https://github.com/gamboozino/ngbrecomp) (`tools/ngbrecomp/xboxrecomp`,
with this project's patches from `patches/`), is MIT licensed. Its `NOTICE`
lists what it contains under other licenses, among them:

| Component | License |
|---|---|
| xemu's NV2A GPU model and Vulkan renderer, MCPX APU | LGPL-2.1-or-later |
| volk, Vulkan Memory Allocator, Vulkan-Headers | MIT / Apache-2.0 |
| SPIRV-Reflect | Apache-2.0 |
| stb | MIT / public domain |
| xxHash | BSD-2-Clause |

The xemu parts are compiled into `Rocky.exe`. Their source, with every
change made here, is in this repository and in `tools/ngbrecomp`, so the
executable can be rebuilt with a modified version of them.

## Libraries beside the executable

| Library | License |
|---|---|
| SDL2 | zlib |
| GLib, libintl, libiconv | LGPL-2.1-or-later |
| PCRE2 | BSD-3-Clause |
| glslang | BSD-3-Clause and others |
| SPIRV-Tools | Apache-2.0 |
| LLVM libc++, libunwind | Apache-2.0 with LLVM exception |
| mingw-w64 winpthreads | MIT and ZPL-2.1 |

The LGPL libraries are separate DLLs and can be replaced by other builds of
the same version.

## Fonts

The launcher's fonts are compiled into `Rocky.exe` (from `assets/fonts/`):
[Bebas Neue](https://github.com/dharmatype/Bebas-Neue) and
[Oswald](https://github.com/googlefonts/OswaldFont), both under the SIL Open
Font License 1.1 (`licenses/OFL-BebasNeue.txt`, `licenses/OFL-Oswald.txt`).

## Build tools

[Capstone](https://www.capstone-engine.org/) (BSD-3-Clause) disassembles the
XBE when generating code; [Ghidra](https://ghidra-sre.org/) (Apache-2.0) names
functions when available. Neither is part of the built game.

## The game

The *Rocky* interactive game is © 2002 Rage Games Limited (developed by Rage
Newcastle Studio, published in North America by Ubi Soft); "Rocky" and the
Rocky films are © United Artists Corporation (MGM). Bink video is © RAD Game
Tools. None of the game's data is in this repository or in a release; its
code is translated from the player's own copy when building. `src/recomp_manual.c` holds hand-written replacements for a few of
the game's functions.
