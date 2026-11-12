# Regenerates every screenshot in assets/screenshots from the CURRENT build, silently and off-screen.
# Part of every release (see docs/RELEASING.md): run it, look at the pictures, commit them with the release.
#   .\scripts\refresh-visuals.ps1            (build first: .\scripts\build.ps1)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$exe  = Join-Path $Root "dist\app\kinoz.exe"
if (-not (Test-Path $exe)) { throw "dist\app\kinoz.exe not found - build first" }
$demo = Join-Path $Root ".toolchain\tmp\demo"
$work = Join-Path $Root ".toolchain\tmp\visuals"
$out  = Join-Path $Root "assets\screenshots"
& "$PSScriptRoot\make-demo-media.ps1" -Out $demo
if (Test-Path $work) { cmd /c rmdir /s /q "$work" }
New-Item -ItemType Directory -Force "$work\data", "$work\shots", "$work\setup", $out | Out-Null

$saved = @{ S = $env:KINOZ_SILENT; O = $env:KINOZ_OFFSCREEN; D = $env:KINOZ_DATA_DIR }
$env:KINOZ_SILENT = '1'; $env:KINOZ_OFFSCREEN = '1'; $env:KINOZ_DATA_DIR = "$work\data"
try {
    $p = Start-Process $exe -ArgumentList '--shots', "`"$work\shots`"", "`"$demo\video\Aurora.avi`"" -PassThru
    if (-not $p.WaitForExit(240000)) { $p.Kill(); throw "screenshots timed out" }
    $p = Start-Process $exe -ArgumentList '--installer-shots', "`"$work\setup`"" -PassThru
    if (-not $p.WaitForExit(120000)) { $p.Kill(); throw "installer screenshots timed out" }
} finally {
    $env:KINOZ_SILENT = $saved.S; $env:KINOZ_OFFSCREEN = $saved.O; $env:KINOZ_DATA_DIR = $saved.D
    Get-Process kinoz -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Root\dist\*" } | Stop-Process -Force
}
$n = 0
foreach ($f in (Get-ChildItem "$work\shots\*.png") + (Get-ChildItem "$work\setup\*.png")) { Copy-Item $f.FullName "$out\$($f.Name)" -Force; $n++ }
Write-Host "refreshed $n images in assets\screenshots"
