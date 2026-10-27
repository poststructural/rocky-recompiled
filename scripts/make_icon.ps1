# Draws the app icon (assets/rocky.ico): a gold "R" on a dark red badge, in
# the colours of the game's title. Sizes 16-256 in one .ico (PNG-compressed
# entries, which Windows Vista and later read).
#
#   powershell -File scripts/make_icon.ps1
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "assets\rocky.ico"
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $r = New-Object System.Drawing.Rectangle 0, 0, ($s - 1), ($s - 1)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $r, ([System.Drawing.Color]::FromArgb(255, 150, 22, 18)), ([System.Drawing.Color]::FromArgb(255, 40, 4, 4)), 90
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $rad = [Math]::Max(2, [int]($s / 5))
    $path.AddArc(0, 0, $rad, $rad, 180, 90); $path.AddArc($s - 1 - $rad, 0, $rad, $rad, 270, 90)
    $path.AddArc($s - 1 - $rad, $s - 1 - $rad, $rad, $rad, 0, 90); $path.AddArc(0, $s - 1 - $rad, $rad, $rad, 90, 90)
    $path.CloseFigure()
    $g.FillPath($bg, $path)
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 230, 190, 90)), ([Math]::Max(1, $s / 32))
    $g.DrawPath($pen, $path)
    $font = New-Object System.Drawing.Font "Arial Black", ([float]($s * 0.62)), ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
    $rf = New-Object System.Drawing.RectangleF 0, ([float]($s * 0.04)), $s, $s
    $gold = New-Object System.Drawing.Drawing2D.LinearGradientBrush $r, ([System.Drawing.Color]::FromArgb(255, 255, 236, 160)), ([System.Drawing.Color]::FromArgb(255, 200, 150, 40)), 90
    $g.DrawString("R", $font, [System.Drawing.Brushes]::Black, (New-Object System.Drawing.RectangleF ([float]($s * 0.03)), ([float]($s * 0.07)), $s, $s), $fmt)
    $g.DrawString("R", $font, $gold, $rf, $fmt)
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    , $ms.ToArray()
}
$fs = [System.IO.File]::Create($out)
$w = New-Object System.IO.BinaryWriter $fs
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $w.Write([byte]($(if ($s -ge 256) { 0 } else { $s }))); $w.Write([byte]($(if ($s -ge 256) { 0 } else { $s })))
    $w.Write([byte]0); $w.Write([byte]0); $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
$w.Close()
"wrote $out"
