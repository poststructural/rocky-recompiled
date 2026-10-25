# Capture one call of a lifted function in a live run and replay it against
# the original machine code (tools/recomp/difftest.py, via work/difftest).
#
#   powershell -File scripts/capture_diff.ps1 -Va 0x000E14B0 -Skip 10 [-Env "RKY_MOVIES=1"] [-Seconds 30]
#
# Needs unicorn and numpy. Prints the difftest verdict.
param([string]$Va, [int]$Skip = 0, [string]$Env = "", [int]$Seconds = 30)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "work\cap_$($Va)_$Skip"
New-Item -ItemType Directory -Force $dir | Out-Null
Remove-Item "$dir\*" -ErrorAction SilentlyContinue
$envs = "RECOMP_CAPTURE=$Va;RECOMP_CAPTURE_SKIP=$Skip;RECOMP_CAPTURE_DIR=$dir"
if ($Env) { $envs = "$Env;$envs" }
powershell -File (Join-Path $PSScriptRoot "shot_run.ps1") -Out "$env:TEMP\capdiff" -At "$Seconds" -Env $envs | Out-Null
if (-not (Test-Path "$dir\post_low.bin")) { "no capture (the call was not reached)"; exit 1 }
Push-Location (Join-Path $root "work\difftest")
python difftest.py ..\..\game\default.xbe $dir $Va 2>&1 | Select-Object -Last 30
Pop-Location
