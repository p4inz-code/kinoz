# Builds assets/readme/hero.png (the README banner) from the current screenshots in assets/screenshots.
#   .\scripts\make-hero.ps1        (run by scripts\refresh-visuals.ps1)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$Root = Split-Path -Parent $PSScriptRoot
$shots = Join-Path $Root "assets\screenshots"
$outFile = Join-Path $Root "assets\readme\hero.png"
$W = 1800; $H = 820

function Round-Image([System.Drawing.Image]$src, [int]$radius) {
    $bmp = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath; $d = $radius * 2
    $path.AddArc(0, 0, $d, $d, 180, 90); $path.AddArc($bmp.Width - $d, 0, $d, $d, 270, 90)
    $path.AddArc($bmp.Width - $d, $bmp.Height - $d, $d, $d, 0, 90); $path.AddArc(0, $bmp.Height - $d, $d, $d, 90, 90); $path.CloseFigure()
    $g.SetClip($path); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $bmp
}

$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.Point]::new(0, 0)), ([System.Drawing.Point]::new(0, $H)), ([System.Drawing.Color]::FromArgb(14, 20, 20)), ([System.Drawing.Color]::FromArgb(8, 8, 6))
$g.FillRectangle($bg, 0, 0, $W, $H)
$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath; $glowPath.AddEllipse(300, -250, 1200, 700)
$glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$glow.CenterColor = [System.Drawing.Color]::FromArgb(70, 40, 160, 160); $glow.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 40, 160, 160))
$g.FillPath($glow, $glowPath)

# a card drawn as a parallelogram (top-left, top-right, bottom-left) with a soft shadow
function Card($name, $tl, $tr, $bl, $radius = 22) {
    $img = [System.Drawing.Image]::FromFile((Join-Path $shots "$name.png"))
    $r = Round-Image $img $radius
    $pts = @([System.Drawing.PointF]::new($tl[0], $tl[1]), [System.Drawing.PointF]::new($tr[0], $tr[1]), [System.Drawing.PointF]::new($bl[0], $bl[1]))
    $br = [System.Drawing.PointF]::new($tr[0] + $bl[0] - $tl[0], $tr[1] + $bl[1] - $tl[1])
    foreach ($i in 1..9) {                                                            # soft shadow
        $a = [int](9 * (10 - $i) / 2); $o = $i * 4
        $sp = @([System.Drawing.PointF]::new($tl[0] - $o, $tl[1] - $o + 18), [System.Drawing.PointF]::new($tr[0] + $o, $tr[1] - $o + 18), [System.Drawing.PointF]::new($br.X + $o, $br.Y + $o + 18), [System.Drawing.PointF]::new($bl[0] - $o, $bl[1] + $o + 18))
        $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($a, 0, 0, 0))), $sp)
    }
    $g.DrawImage($r, [System.Drawing.PointF[]]$pts)
    $img.Dispose(); $r.Dispose()
}

# back row (tilted), then the front windows
# sizes keep each screenshot's 1770x1080 proportions (height = width x 0.610)
Card 'settings-light'      @(24, 205)    @(664, 158)   @(48, 595)
Card 'music-album-light'   @(1136, 150)  @(1776, 196)  @(1112, 540)
Card 'home-dark'           @(330, 52)    @(1470, 52)   @(330, 748) 26
Card 'palette-dark'        @(96, 438)    @(696, 402)   @(118, 804)
Card 'playlists-light'     @(1190, 452)  @(1780, 494)  @(1168, 812)
$g.Dispose()
$bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Host "hero written: $outFile"
