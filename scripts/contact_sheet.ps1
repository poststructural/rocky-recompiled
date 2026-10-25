# Tile screenshots into one image (2 columns, 480x280 each) for a quick look.
#
#   powershell -File scripts/contact_sheet.ps1 <out.png> <in1.png> [<in2.png> ...]
param([string]$Out, [Parameter(ValueFromRemainingArguments = $true)][string[]]$In)
Add-Type -AssemblyName System.Drawing
$rows = [math]::Ceiling($In.Count / 2)
$sheet = New-Object System.Drawing.Bitmap 960, (280 * $rows)
$g = [System.Drawing.Graphics]::FromImage($sheet)
for ($i = 0; $i -lt $In.Count; $i++) {
    $b = [System.Drawing.Bitmap]::FromFile($In[$i])
    $g.DrawImage($b, ($i % 2) * 480, [math]::Floor($i / 2) * 280, 480, 280)
    $b.Dispose()
}
$sheet.Save($Out)
$g.Dispose(); $sheet.Dispose()
