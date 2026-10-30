# Rocky — Static Recompilation

A static recompilation of the original Xbox release of *Rocky* (Rage Games /
Ubi Soft, 2002) into a native executable, built on
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and
[ngbrecomp](https://github.com/gamboozino/ngbrecomp)'s xboxrecomp fork. The
pipeline and Vulkan build are carried over from
AnimaniacsRecompiled.

**No game files are included.** You need your own copy of the game.

## Status: playable (Windows, early)

Boots through the intro movies to the title screen, menus, Movie Mode career
setup and exhibition matches, at 60 fps with music, on the Vulkan build.

- Fights in widescreen and ultrawide through the game's own 16:9 mode
  (Hor+); menus and movies at 4:3 with side bars.
- **Launcher** before the game, in the same look: Play, PC Options, Mods
  (switch each on or off), Game Files (install from your disc image), Updates
  (from the GitLab releases; saves, settings and mods are kept), Open Saves
  Folder. `Rocky.exe --play` skips it, `--update` updates from the command
  line; `skip_launcher = on` in settings.ini goes straight to the game.
- First run asks for your disc: the ISO (plain or a full Redump dump), the
  .zip/.7z it came in, or the `default.xbe` of an extracted copy. It checks
  that it is the USA retail release and installs the files once to
  `%LOCALAPPDATA%\RockyRecompiled\game` (`Rocky.exe --install <image>` does
  the same without asking).
- **PC OPTIONS** screen over the paused game, in the look of the game's own
  menus: F1, or Back + Start on a controller. Display mode, window size,
  aspect ratio, render resolution, texture filtering, V-Sync, FPS counter,
  skip intro, and Quit to Desktop; most apply at once, all are saved.
- **PC wording**: the game's save messages talk about your PC and its disk
  instead of the Xbox hard disk and memory units, in all five languages.
  The reworded text is made at start-up from your own copy of the game.
- **Mods**: put a mod in `mods/<name>/` beside the exe, with files at their
  disc paths (e.g. `mods/MyRing/ARENAS/MADISON/MADISON.PAK`); they replace
  the game's files. Mods apply in name order, a later one winning; a file
  named `disabled` in a mod's folder switches it off. Changed archives are
  re-copied into the game's hard-disk cache automatically.
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
git clone --recursive https://github.com/poststructural/rocky-recompiled.git
cd rocky-recompiled
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

## Thanks

None of this would exist without other people's work, and they have our
deepest thanks:

- **sp00nznet**, for [xboxrecomp](https://github.com/sp00nznet/xboxrecomp),
  the toolkit that lifts the XBE to C and runs it.
- **gamboozino**, for [ngbrecomp](https://github.com/gamboozino/ngbrecomp)
  and its xboxrecomp fork with xemu's NV2A model and Vulkan renderer, which
  this build runs on.
- The **[xemu](https://xemu.app/)** developers, whose GPU and audio models
  are inside `Rocky.exe`.
- **TGP482** ([SCDA-XBERecomp](https://github.com/TGP482/SCDA-XBERecomp),
  [BLACK-XBERecomp](https://github.com/TGP482/BLACK-XBERecomp)) and
  **linkssy2** ([Darkwatch Darker Edition](https://github.com/linkssy2/DarkwatchDarkerEdition),
  [Urban Chaos Burner Edition](https://github.com/linkssy2/UrbanChaosRiotResponseBurnerEdition)),
  whose ports set the bar for what an Xbox recomp should feel like on PC.

## Where to find it

Source: [github.com/poststructural/rocky-recompiled](https://github.com/poststructural/rocky-recompiled).
Releases and updates: [gitlab.com/unowa001/rocky-recomp](https://gitlab.com/unowa001/rocky-recomp/-/releases)
(the launcher's updater reads these).

## Licenses

The project's own code is MIT licensed ([LICENSE](LICENSE)). Third-party parts and their licenses are listed in
[THIRD_PARTY.md](THIRD_PARTY.md). *Rocky* belongs to its rights holders; no
game data is included here.
