# Release package: RockyRecompiled-<version>.zip and .zip.sha256 in dist/.
#
#   powershell -File scripts/package.ps1 [-Build build-vk] [-SkipBuild]
#
# The version is vk/CMakeLists.txt's project version. The zip holds one
# folder, RockyRecompiled-<version>/, laid out as the updater expects
# (src/update.c): Rocky.exe and its DLLs, Rocky.pdb (names in crash
# reports), licenses/, README.txt, THIRD_PARTY.md, docs/, mods/README.txt.
# Never settings.ini, saves, mods or game files.
param([string]$Build = "build-vk", [switch]$SkipBuild)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cm = Get-Content (Join-Path $root "vk\CMakeLists.txt") -Raw
if ($cm -notmatch 'project\(rocky_vk VERSION (\d+\.\d+\.\d+)') { throw "no version in vk/CMakeLists.txt" }
$ver = $Matches[1]
$name = "RockyRecompiled-$ver"
$bdir = Join-Path $root $Build

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build_vk.ps1")
    if ($LASTEXITCODE) { throw "build failed" }
}

$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
if (Test-Path $stage) { cmd /c "rmdir /s /q `"$stage`"" }
New-Item -ItemType Directory -Force "$stage\licenses", "$stage\docs", "$stage\mods" | Out-Null

Copy-Item "$bdir\Rocky.exe", "$bdir\Rocky.pdb" $stage
Copy-Item "$bdir\*.dll" $stage
Copy-Item "$root\THIRD_PARTY.md" $stage
Copy-Item "$bdir\licenses\*" "$stage\licenses"
$lic = Join-Path $root "work\windeps\sysroot\clang64\share\licenses"
$map = @{ "SDL2\LICENSE.txt" = "SDL2-LICENSE.txt"; "glib2\COPYING" = "GLib-COPYING.txt";
          "gettext-runtime\COPYING" = "gettext-COPYING.txt"; "libiconv\COPYING.LIB" = "libiconv-COPYING.LIB.txt";
          "glslang\LICENSE.txt" = "glslang-LICENSE.txt"; "libc++\LICENSE" = "libcxx-LICENSE.txt";
          "libunwind\LICENSE" = "libunwind-LICENSE.txt"; "pcre2\LICENCE.md" = "PCRE2-LICENCE.txt";
          "spirv-tools\LICENSE" = "SPIRV-Tools-LICENSE.txt" }
foreach ($k in $map.Keys) { Copy-Item (Join-Path $lic $k) (Join-Path "$stage\licenses" $map[$k]) }
$llvm = Get-ChildItem (Join-Path $root "work\windeps") -Directory -Filter "llvm-mingw-*" | Select-Object -First 1
Copy-Item (Join-Path $llvm.FullName "LICENSE.TXT") "$stage\licenses\llvm-mingw-LICENSE.txt"
if (Test-Path "$root\LICENSE") { Copy-Item "$root\LICENSE" "$stage\licenses\RockyRecompiled-LICENSE.txt" }
foreach ($d in "SETTINGS.md", "CONTROLS.md") {
    if (Test-Path "$root\docs\$d") { Copy-Item "$root\docs\$d" "$stage\docs" }
}

@"
Rocky Recompiled $ver
=====================

An unofficial native PC version of Rocky (original Xbox, 2002), made by
static recompilation. It contains none of the game's data: you need your
own copy of Rocky for the original Xbox (USA).

Start Rocky.exe. The launcher asks for your disc image the first time
(.iso, the .zip/.7z it came in, or the default.xbe of an extracted copy)
and installs the game files once.

  F1 (or Back + Start)   PC options in game
  F10                    frame rate counter
  F11 / Alt+Enter        fullscreen

Settings: settings.ini beside Rocky.exe (docs/SETTINGS.md).
Controls: docs/CONTROLS.md. Mods: mods/README.txt.
Saves and log.txt: %LOCALAPPDATA%\RockyRecompiled.

Source and releases: https://gitlab.com/unowa001/rocky-recomp
"@ | Set-Content "$stage\README.txt" -Encoding ascii

@"
Mods
====

Each mod is a folder in here holding files at the same paths as on the
disc, which then replace the game's own, e.g.

  mods\MyRing\ARENAS\MADISON\MADISON.PAK

Mods apply in folder-name order; when two replace the same file, the later
one wins. A file named "disabled" in a mod's folder switches it off (the
launcher's Mods page does this for you). Changes apply the next time the
game starts.
"@ | Set-Content "$stage\mods\README.txt" -Encoding ascii

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
Push-Location $dist
& "$env:SystemRoot\System32\tar.exe" -a -cf "$name.zip" $name
Pop-Location
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
Set-Content "$zip.sha256" "$hash  $name.zip" -Encoding ascii
"{0}  ({1:N1} MB)`nsha256 {2}" -f $zip, ((Get-Item $zip).Length / 1MB), $hash
