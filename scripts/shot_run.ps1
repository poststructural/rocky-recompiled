# Run the game for a while and screenshot it at given times (bring-up aid).
#
#   powershell -File scripts/shot_run.ps1 -Out <prefix> -At 15,30 [-Env "NAME=value;NAME2=value"]
#
# Writes <prefix>_t<seconds>.png for each time, stdout/stderr to
# build-vk/vk_stdout.txt and build-vk/vk_stderr.txt, then ends the game.
param([string]$Out, [string]$At = "15", [string]$Env = "")
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-vk"
foreach ($kv in ($Env -split ";" | Where-Object { $_ })) {
    $k, $v = $kv -split "=", 2
    Set-Item -Path "Env:$k" -Value $v
}
$p = Start-Process (Join-Path $build "Rocky.exe") -WorkingDirectory $build -PassThru `
    -RedirectStandardOutput (Join-Path $build "vk_stdout.txt") `
    -RedirectStandardError (Join-Path $build "vk_stderr.txt")
$elapsed = 0
foreach ($t in ($At -split "," | ForEach-Object { [int]$_ } | Sort-Object)) {
    Start-Sleep -Seconds ($t - $elapsed)
    $elapsed = $t
    if ($p.HasExited) { "game exited (code $($p.ExitCode)) before ${t}s"; break }
    & (Join-Path $PSScriptRoot "winshot.ps1") "${Out}_t$t.png"
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
