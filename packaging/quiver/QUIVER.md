# Rocky Recompiled 1.0.0 in Quiver Launcher

Quiver Launcher (github.com/tgeorgiadis/quiver-launcher, v3.5.1) installs apps
from GitHub or gitlab.com release assets. The v1.0.0 GitLab release already
fits what it expects, so no new build is needed:

- Asset `RockyRecompiled-1.0.0.zip` is a release link hosted on gitlab.com
  (Quiver only accepts gitlab.com-hosted links). An unlabeled .zip counts as
  the Windows build; the name has no "xbox" token, which Quiver would treat as
  a console-only package and skip. The `.zip.sha256` sidecar is ignored as an
  auxiliary file.
- The zip holds one folder, `RockyRecompiled-1.0.0/`, with `Rocky.exe` as the
  only executable. Quiver flattens a single wrapper folder, so it installs to
  `Apps/Rocky-RockyRecompiled/Rocky.exe` and Play starts it.
- Updates extract over the old files, so `settings.ini` and `mods/` stay.
  Saves and installed game files live in `%LOCALAPPDATA%\RockyRecompiled`
  and are untouched.
- First Play opens Rocky's own launcher, which asks for the disc image once,
  as with a normal install.

Checked: downloaded the zip from the release, SHA-256 matches
`da785d48b66bbb61bd59d549d4cd1f7db5ef5a78081aba7e18e3b4964f104e0a`; layout as
above. Not checked: an actual install in Quiver (no Quiver run yet).

## Use it now (My app list)

Quiver: Settings > Advanced > My app list, point it at `apps.json` (a local
file, or the raw GitLab URL once it is pushed:
https://gitlab.com/unowa001/rocky-recomp/-/raw/main/packaging/quiver/apps.json).
Rocky then shows under My app list in the App Catalog; add it and Install.

Or + Add New Entry with: Name `Rocky`, Project `Rocky Recompiled`,
Repository `unowa001/rocky-recomp`, Source GitLab, Folder `Rocky-RockyRecompiled`.

`appIconUrl` points at `assets/rocky-icon.png` on main (256x256, from
rocky.ico); it shows the default icon until that file is pushed.

## Getting into the quiverlauncher.com catalog

The catalog is curated on quiverlauncher.com (it already lists several XBE
recomps, e.g. SCDA-XBERecomp, Black, PGR2). Submission is at
https://quiverlauncher.com/submit (needs a sign-in there). Suggested text:

> **Rocky Recompiled** - an unofficial native Windows version of Rocky
> (original Xbox, 2002) by static recompilation. GitLab: unowa001/rocky-recomp.
> MIT. Contains no game data: the first start asks for your own USA disc image
> and installs the game files once. Windows 10/11 x64, Vulkan 1.3.
> Tags: xbox, recomp.

## Caveat

Rocky's own launcher also has an Updates page. If a player updates there,
Quiver still thinks the old version is installed until it updates again.
Harmless (same files), but updating from Quiver is the cleaner path.
