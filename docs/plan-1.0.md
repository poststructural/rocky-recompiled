# Rocky Recompiled 1.0 plan

What is left between today's build (playable, installer, PC OPTIONS) and a
1.0 release on GitLab. LAN multiplayer is built in its own worktree
(thread "Add LAN multiplayer to Rocky") and merges in before 1.0; the items
here avoid the files it changes most (the netplay code, the pad path) and
merge through `main`.

## 1. Mods (file overrides)

- `mods/<name>/` beside the exe (or in the save folder): any file there
  replaces the disc file of the same path (`.PAK`, `.bik`, audio). Mods load
  in name order; `mods/<name>/disabled` turns one off.
- Done in the runtime's path translation (toolkit patch `ngbrecomp-0009`):
  game-disc paths are looked up in each enabled mod folder first.
- The launcher lists mods with on/off switches; the log names every override
  used.

## 2. Launcher

A window before the game, in the same gold-on-dark look as PC OPTIONS:

- **Play** (Enter / A), **Settings** (every PC OPTIONS row plus MSAA, render
  scale cap, controller check), **Mods**, **Game files** (shows the install,
  reinstall or point at another copy), **Check for updates**, **Open saves
  folder**, **Quit**.
- `--play` (and the game's own re-launch) skips it; a "Skip launcher" switch
  in settings for people who just want the game.
- Shows the version and, when the update check finds one, a notice.

## 3. Updater

- Reads the GitLab releases of the project
  (`/api/v4/projects/<id>/releases`), newest first. Assets follow the
  Crackdown launcher's naming: `RockyRecompiled-<ver>.zip` and
  `RockyRecompiled-<ver>.zip.sha256`.
- Automatic check at launcher start (can be turned off), manual button. On
  "Update": download to the save folder, check the SHA-256, then a small
  helper (`Rocky.exe --apply-update <zip>`) waits for the launcher to exit,
  swaps the files (old ones kept as `.old` until the next start) and starts
  the new launcher. Saves, settings and mods are never touched.

## 4. Release packaging

- `scripts/package.ps1 -Version 1.0.0`: clean Release build, the exe, its
  DLLs, licenses, README, `docs/SETTINGS.md`, `CONTROLS.md`; zip + `.sha256`.
- Version from one place (`vk/CMakeLists.txt` project version), shown in the
  launcher, the window title and crash reports.
- App icon and version resource for the exe.
- Release notes and a GitLab release (Austin runs the `glab release create`
  command, or allows `glab` for this session).

## 5. Polish and open issues

- Audio peaks clip: find the mix gain (APU voice volume or output scale).
- Pause menu and other 2D during matches stretch on wide screens: keep them
  at 4:3 like the menus.
- Unattended long run: a full exhibition fight to KO, the career's first
  training games and the second fight, saving and loading a career.
- Portable mode (`portable.txt` beside the exe keeps saves/settings there).
- docs: `SETTINGS.md`, `CONTROLS.md` (keyboard, controller, F1/F10/F11).

## Order

Mods → launcher (with the settings and game-files pages) → updater →
polish → packaging → merge LAN multiplayer → 1.0.

## Open question for Austin

The repository has no license yet; the project's own code needs one for a
public 1.0. The runtime it builds on (xboxrecomp, in ngbrecomp's copy) is MIT
with LGPL-2.1 xemu parts (THIRD_PARTY.md); none of ngbrecomp's GPL-2.0 game
code is used. MIT is the simple match.
