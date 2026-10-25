# Capture one call each of several lifted functions in a single live run and
# replay every one against the original code (see capture_diff.ps1).
#
#   powershell -File scripts/capture_diff_list.ps1 -Vas "0x000E14B0,0x000E3270" -Skip 20 [-Env ...]
param([string]$Vas, [int]$Skip = 0, [string]$Env = "", [int]$Seconds = 40)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "work\caplist_$Skip"
New-Item -ItemType Directory -Force $dir | Out-Null
$envs = "RECOMP_CAPTURE=$Vas;RECOMP_CAPTURE_SKIP=$Skip;RECOMP_CAPTURE_DIR=$dir"
if ($Env) { $envs = "$Env;$envs" }
powershell -File (Join-Path $PSScriptRoot "shot_run.ps1") -Out "$env:TEMP\capdiff" -At "$Seconds" -Env $envs | Out-Null
Push-Location (Join-Path $root "work\difftest")
foreach ($va in ($Vas -split ",")) {
    $sub = Join-Path $dir ("{0:X8}" -f [Convert]::ToUInt32($va, 16))
    if (-not (Test-Path "$sub\post_low.bin")) { "== $va : not reached"; continue }
    "== $va : " + ((python difftest.py ..\..\game\default.xbe $sub $va 2>&1 | Select-Object -Last 4) -join " / ")
}
Pop-Location
