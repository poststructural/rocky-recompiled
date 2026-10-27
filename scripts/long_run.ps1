# Unattended long run, headless (no window, nothing takes the screen):
# boot, Movie Mode career start, then punch through fights with a scripted
# pad, logging the main loop's state (0x0010A70C) and checking for crashes.
#
#   powershell -File scripts/long_run.ps1 [-Minutes 10] [-Build build-dev]
#
# Uses its own save folder (work/longrun-save). The log is
# <build>/longrun_err.txt; the state timeline is printed as it changes.
param([int]$Minutes = 10, [string]$Build = "build-dev")
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $Build
$save = Join-Path $root "work\longrun-save"
New-Item -ItemType Directory -Force $save | Out-Null

# Pad timeline (ms from enumeration): Start at the title, A through Movie
# Mode -> Begin New Game -> difficulty -> name "Done" -> the first fight,
# then punches (A, B, X, Y) and the odd Start/A to get past result screens.
$steps = @("20000:start", "24000:a", "27000:down", "28000:a", "31000:a", "34000:a",
           "37000:a", "40000:a", "44000:a", "48000:a", "52000:a")
$t = 56000
$buttons = @("a", "b", "x", "y", "a", "b", "x", "lup+a", "y", "b")
$end = $Minutes * 60000
$i = 0
while ($t -lt $end) {
    $steps += "${t}:$($buttons[$i % $buttons.Count])"
    if ($i % 40 -eq 39) { $steps += "$($t + 150):start"; $steps += "$($t + 1200):a" }
    $t += 350
    $i++
}
$script = Join-Path $root "work\longrun-pad.txt"
Set-Content $script ($steps -join "`n") -Encoding ascii

$env:RKY_RENDERER = "null"
$env:RKY_SAVE_DIR = $save
$env:RKY_NO_MOVIES = "1"
$env:RECOMP_PAD_SCRIPT = "@$script"
$err = Join-Path $build "longrun_err.txt"
$p = Start-Process (Join-Path $build "Rocky.exe") -ArgumentList "--play `"$root\game`"" `
    -WorkingDirectory $build -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $build "longrun_out.txt") -RedirectStandardError $err
$clock = [Diagnostics.Stopwatch]::StartNew()
$last = ""
while (-not $p.HasExited -and $clock.Elapsed.TotalMinutes -lt $Minutes + 0.5) {
    Start-Sleep 5
    $m = Select-String $err -Pattern "offset \+(\d+) from Xbox base" | Select-Object -First 1
    if ($m) {
        $env:RKY_MEM_BASE = "{0:X}" -f [uint64]$m.Matches[0].Groups[1].Value
        $s = (python (Join-Path $PSScriptRoot "guest_mem.py") peek 0x0010A70C 1) -replace '^\S+\s+', ''
        if ($s -ne $last) { "{0,6:N0}s  loop state {1}" -f $clock.Elapsed.TotalSeconds, $s; $last = $s }
    }
}
if ($p.HasExited) { "game exited (code $($p.ExitCode)) after $([int]$clock.Elapsed.TotalSeconds)s" }
else { Stop-Process -Id $p.Id -Force; "ran $Minutes min without crashing" }
Select-String $err -Pattern "CRASH|ICALL\] F|UNIMPL" | Select-Object -First 8 | ForEach-Object Line
Get-ChildItem (Join-Path $save "UserData") -Recurse -File -ErrorAction SilentlyContinue |
    Select-Object @{n = "save file"; e = { $_.FullName.Substring($save.Length) } }, Length | Format-Table -AutoSize
