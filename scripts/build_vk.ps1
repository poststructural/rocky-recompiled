# Rebuild the Vulkan version on Windows (bring-up loop).
#
#   powershell -File scripts/build_vk.ps1 [-Regen]
#
# -Regen first re-runs scripts/regen.sh (needed after changing
# config/seed_functions.json or adding/removing an override in
# src/recomp_manual.c). Logs go to work/regen.log and work/build.log.
# CMake and Ninja come from Visual Studio unless they are on PATH already.
param([switch]$Regen)
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $cm = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake"
    $env:PATH = "$cm\CMake\bin;$cm\Ninja;$env:PATH"
}
if ($Regen) {
    $env:TK = "tools/ngbrecomp/xboxrecomp"
    if (-not $env:XBE) { $env:XBE = "game/default.xbe" }
    & "$env:ProgramFiles\Git\bin\bash.exe" scripts/regen.sh *> work\regen.log
    if ($LASTEXITCODE) { "regen failed, see work/regen.log"; exit 1 }
    Select-String work\regen.log -Pattern "functions \(" | ForEach-Object Line
}
if (-not (Test-Path build-vk\build.ninja)) {
    cmake -S vk -B build-vk -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$root/cmake/toolchains/windows-llvm-mingw.cmake" *> work\configure.log
}
cmd /c "ninja -C build-vk > work\build.log 2>&1"
if ($LASTEXITCODE) {
    Select-String work\build.log -Pattern " error:" | Select-Object -First 10 | ForEach-Object Line
    "build failed, see work/build.log"; exit 1
}
"build ok"
