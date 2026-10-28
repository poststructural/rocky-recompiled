# Rocky Recompiled 1.0 plan

What is left between the playable build and a 1.0 release on GitLab. LAN
multiplayer (worktree `RockyRecompiled-net`, branch `netplay`) is out of
scope for 1.0 and ships later.

## Done

- **Mods** (`src/mods.c`, runtime patch `ngbrecomp-0009`): `mods/<name>/`
  files replace the disc's; on/off per mod; changed archives re-cached.
- **Launcher** (`src/launcher.c`, `src/ui.c`): Play, PC Options, Mods, Game
  Files, Updates, Open Saves Folder, Quit; keys, mouse, XInput; `--play`,
  `--launcher`, `skip_launcher`.
- **Updater** (`src/update.c`): GitLab releases of project 88056818, SHA-256
  checked, files swapped with `.old` kept until the next start; player files
  refused; `--update`; tested with a local feed (`RKY_UPDATE_FEED`).
- **Packaging** (`scripts/package.ps1`): zip + sha256 in the updater's
  layout, licenses for every bundled library; icon and version resource.
- **Polish**: audio peaks soft-limited (`ngbrecomp-0020`), portable mode,
  `docs/SETTINGS.md`, `docs/CONTROLS.md`, THIRD_PARTY credits.
- **Tests**: headless scripted-pad runs (`scripts/long_run.ps1`).

## Left

- A license for the project's own code (Austin's choice; MIT suggested).
- Known minor issue, listed in the release notes and left for 1.0.1: the
  in-match pause menu is 2D drawn at the window's shape (the match HUD uses
  the game's own 16:9 layout and is fine). Fixing it means finding the 2D
  draw path and squeezing its vertices, which needs screenshot runs.
- Release: bump the version in `vk/CMakeLists.txt` if needed, run
  `scripts/package.ps1`, tag `v1.0.0`, create the GitLab release with the
  two asset links named `RockyRecompiled-1.0.0.zip` and
  `RockyRecompiled-1.0.0.zip.sha256` (the updater looks for exactly these).

## Releasing (commands)

```
powershell -File scripts/package.ps1
git tag v1.0.0 && git push gitlab v1.0.0
glab release create v1.0.0 dist/RockyRecompiled-1.0.0.zip dist/RockyRecompiled-1.0.0.zip.sha256 \
    --repo unowa001/rocky-recomp --name "Rocky Recompiled 1.0.0" --notes-file docs/release-notes-1.0.0.md
```

`glab release create` uploads the files as release asset links with their
file names, which is what `src/update.c` matches.
