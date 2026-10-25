# Rocky — Static Recompilation

A static recompilation of the original Xbox release of *Rocky* (Rage Games /
Ubi Soft, 2002) into a native executable, built on
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and
[ngbrecomp](https://github.com/gamboozino/ngbrecomp)'s xboxrecomp fork. The
pipeline and Vulkan build are carried over from
[AnimaniacsRecompiled](https://github.com/Unowa001/AnimaniacsRecompiled).

**No game files are included.** You need your own copy of the game.

## Status: playable (Windows, early)

Boots through the intro movies to the title screen, menus, Movie Mode career
setup and exhibition matches, at 60 fps with music, on the Vulkan build.

- Fights in widescreen and ultrawide through the game's own 16:9 mode
  (Hor+); menus and movies at 4:3 with side bars.
- First run asks for your disc: the ISO (plain or a full Redump dump), the
  .zip/.7z it came in, or the `default.xbe` of an extracted copy. It checks
  that it is the USA retail release and installs the files once to
  `%LOCALAPPDATA%\RockyRecompiled\game` (`Rocky.exe --install <image>` does
  the same without asking).
- `settings.ini` beside the exe: window or fullscreen, resolution, aspect,
  render scale, `skip_intro` (skips the three boot movies).
- Keyboard controls as in AnimaniacsRecompiled (Enter = Start, Space/K = A,
  L = B, arrows = D-pad); controllers through SDL (tested).

Bring-up notes, the fixes and the debugging tools are in
[docs/NOTES.md](docs/NOTES.md).

## The game binary

| | |
|---|---|
| Title ID | `0x55530014` (USA, retail) |
| Build | 2002-08-14, XDK 4721 |
| Debug path | `C:\Boxing\Game\Game.exe` |
| Entry point | `0x0009AE9C` |
| Code | `.text` 0.6 MB + statically linked D3D8, XGRPH, DSOUND, XPP and Bink sections |
| Kernel imports | 121 |

## Building (Windows, Vulkan)

Requirements: Python 3.10+ with `capstone`, CMake 3.20+, Ninja, a GPU with
Vulkan 1.3 drivers. Optional: Ghidra 12.x and a 64-bit JDK 21 for function
naming.

```bash
git clone --recursive https://github.com/Unowa001/RockyRecompiled.git
cd RockyRecompiled
pip install capstone

# 1. Extract your disc into game/
python -m tools.xiso unpack "Rocky (USA).iso" -o game   # run from tools/xboxrecomp

# 2. One-time setup: fork patches, llvm-mingw, libraries
scripts/setup_vk.sh

# 3. Generate the C from your XBE
GHIDRA_HOME=/path/to/ghidra_12.1.4_PUBLIC JAVA_HOME=/path/to/jdk-21 \
    TK=tools/ngbrecomp/xboxrecomp XBE=game/default.xbe scripts/regen.sh

# 4. Build
cmake -S vk -B build-vk -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-llvm-mingw.cmake
ninja -C build-vk

# 5. Run; the first start asks for your disc image (or put game/ beside the exe)
build-vk/Rocky.exe
```

Saves and `log.txt` go to `%LOCALAPPDATA%\RockyRecompiled` (`RKY_SAVE_DIR`
overrides).

## Layout

```
src/main_vk.c           host entry: window, GPU model, virtual CPU
src/recomp_manual.c     hand-written overrides for lifted functions
src/settings.c          settings.ini
src/recomp/gen/         generated C (not committed; produced by scripts/regen.sh)
tools/xboxrecomp/       the toolkit (git submodule)
tools/ngbrecomp/        ngbrecomp, for its xboxrecomp fork (git submodule)
vk/                     the Vulkan build
patches/                local patches applied to the toolkits
scripts/regen.sh        the full XBE -> C pipeline
docs/NOTES.md           bring-up notes
```

## Licenses

Third-party parts and their licenses are listed in
[THIRD_PARTY.md](THIRD_PARTY.md). *Rocky* belongs to its rights holders; no
game data is included here.
