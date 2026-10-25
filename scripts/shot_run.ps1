# Run the game, press keys and take screenshots on a timeline (bring-up aid).
#
#   powershell -File scripts/shot_run.ps1 -Out <prefix> -At "15,30" [-Steps "16=RETURN,20=SPACE"]
#                                         [-Env "NAME=value;NAME2=value"]
#
# -At: seconds at which to save <prefix>_t<seconds>.png.
# -Steps: seconds=key presses, sent to the game window, or seconds=dump
#   (guest .data/BSS to <prefix>_t<seconds>.bin) (keys are .NET
#   System.Windows.Forms.Keys names: RETURN, SPACE, UP, DOWN, LEFT, RIGHT,
#   BACK, ESCAPE, K, L ...; see docs for the keyboard controls).
# -Build: the build folder to run (default build-vk); the game is the repo's game/.
# stdout/stderr go to <build>/vk_stdout.txt and build-vk/vk_stderr.txt; the
# game is ended after the last step.
param([string]$Out, [string]$At = "15", [string]$Steps = "", [string]$Env = "", [string]$Build = "build-vk")
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $Build
foreach ($kv in ($Env -split ";" | Where-Object { $_ })) {
    $k, $v = $kv -split "=", 2
    Set-Item -Path "Env:$k" -Value $v
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class RkyKeys {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
}
"@
$events = @()
foreach ($t in ($At -split "," | Where-Object { $_ })) { $events += [pscustomobject]@{ T = [double]$t; What = "shot" } }
foreach ($s in ($Steps -split "," | Where-Object { $_ })) {
    $t, $key = $s -split "=", 2
    $events += [pscustomobject]@{ T = [double]$t; What = $key }
}
$events = $events | Sort-Object T
$p = Start-Process (Join-Path $build "Rocky.exe") -ArgumentList "`"$root\game`"" -WorkingDirectory $build -PassThru `
    -RedirectStandardOutput (Join-Path $build "vk_stdout.txt") `
    -RedirectStandardError (Join-Path $build "vk_stderr.txt")
$clock = [Diagnostics.Stopwatch]::StartNew()
foreach ($e in $events) {
    $wait = $e.T - $clock.Elapsed.TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    if ($p.HasExited) { "game exited (code $($p.ExitCode)) before $($e.T)s"; break }
    if ($e.What -eq "shot") {
        & (Join-Path $PSScriptRoot "winshot.ps1") "${Out}_t$($e.T).png" -ProcessId $p.Id
    } elseif ($e.What -eq "dump") {
        # Guest .data + BSS, for scripts/guest_mem.py-style diffs between states.
        $env:RKY_MEM_BASE = ""
        python (Join-Path $PSScriptRoot "guest_mem.py") dump "${Out}_t$($e.T).bin" 108680 4D5748
    } else {
        $p.Refresh()
        [RkyKeys]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
        $vk = [byte][Enum]::Parse([System.Windows.Forms.Keys], $e.What, $true)
        $scan = [byte][RkyKeys]::MapVirtualKey($vk, 0)
        # Arrows and the navigation block are extended keys; without the flag
        # Windows reports them as the numpad's.
        $ext = if (@(33..40 + 45 + 46) -contains $vk) { 1 } else { 0 }
        [RkyKeys]::keybd_event($vk, $scan, $ext, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 120
        [RkyKeys]::keybd_event($vk, $scan, $ext -bor 2, [UIntPtr]::Zero)
        "pressed $($e.What) at $($e.T)s"
    }
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
