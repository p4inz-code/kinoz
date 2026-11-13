# Builds assets/readme/hero.png (the README banner) from the current screenshots in assets/screenshots.
#   .\scripts\make-hero.ps1        (run by scripts\refresh-visuals.ps1)
# GitHub shows the banner only ~850 px wide, so it is one large, real window plus a short line of big type:
# everything must stay readable at less than half size.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$Root = Split-Path -Parent $PSScriptRoot
$shots = Join-Path $Root "assets\screenshots"
$outFile = Join-Path $Root "assets\readme\hero.png"
$W = 1800; $H = 960

function Round-Image([System.Drawing.Image]$src, [int]$radius) {
    $bmp = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath; $d = $radius * 2
    $path.AddArc(0, 0, $d, $d, 180, 90); $path.AddArc($bmp.Width - $d, 0, $d, $d, 270, 90)
    $path.AddArc($bmp.Width - $d, $bmp.Height - $d, $d, $d, 0, 90); $path.AddArc(0, $bmp.Height - $d, $d, $d, 90, 90); $path.CloseFigure()
    $g.SetClip($path); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $bmp
}

$fonts = New-Object System.Drawing.Text.PrivateFontCollection
foreach ($f in 'AtkinsonHyperlegibleNext-Bold.ttf', 'AtkinsonHyperlegibleNext-Regular.ttf') { $fonts.AddFontFile((Join-Path $Root "resources\fonts\$f")) }
$fam = $fonts.Families[0]

$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.TextRenderingHint = 'AntiAliasGridFit'
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.Point]::new(0, 0)), ([System.Drawing.Point]::new(0, $H)), ([System.Drawing.Color]::FromArgb(16, 24, 24)), ([System.Drawing.Color]::FromArgb(8, 9, 8))
$g.FillRectangle($bg, 0, 0, $W, $H)
$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath; $glowPath.AddEllipse(700, -300, 1400, 900)
$glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$glow.CenterColor = [System.Drawing.Color]::FromArgb(60, 40, 170, 170); $glow.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 40, 170, 170))
$g.FillPath($glow, $glowPath)

# the real window, large and straight, with a soft shadow
$img = [System.Drawing.Image]::FromFile((Join-Path $shots "hero-dark.png"))
$winW = 1250; $winH = [int]($winW * $img.Height / $img.Width); $winX = $W - $winW - 40; $winY = [int](($H - $winH) / 2)
foreach ($i in 1..12) {
    $a = [int](7 * (13 - $i) / 2); $o = $i * 5
    $rect = [System.Drawing.RectangleF]::new($winX - $o, $winY - $o + 22, $winW + 2 * $o, $winH + 2 * $o)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($a, 0, 0, 0))), $rect)
}
$rounded = Round-Image $img 22
$g.DrawImage($rounded, $winX, $winY, $winW, $winH)
$g.DrawRectangle((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(46, 255, 255, 255), 1.5)), $winX, $winY, $winW, $winH)
$img.Dispose(); $rounded.Dispose()

# type: big enough to read at half size
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(246, 246, 240))
$muted = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(176, 190, 188))
$accent = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(79, 209, 214))
$title = New-Object System.Drawing.Font $fam, 100, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$tag = New-Object System.Drawing.Font $fam, 37, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$body = New-Object System.Drawing.Font $fam, 26, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$small = New-Object System.Drawing.Font $fam, 26, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$x = 56; $textW = $winX - $x - 36
$g.DrawString("kinoz", $title, $white, $x - 4, 250)
$fmt = New-Object System.Drawing.StringFormat
$g.DrawString("Everything you play,`nin one calm window.", $tag, $accent, [System.Drawing.RectangleF]::new($x, 388, $textW, 120), $fmt)
$g.DrawString("Videos, music, images, GIFs.`nFree. Offline. Private.", $body, $muted, [System.Drawing.RectangleF]::new($x, 506, $textW, 100), $fmt)
$g.DrawString("Windows  $([char]0x00B7)  macOS  $([char]0x00B7)  Linux", $small, $white, [System.Drawing.RectangleF]::new($x, 650, $textW, 40), $fmt)
$g.Dispose()
$bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Host "hero written: $outFile"
