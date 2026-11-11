# Dot-source this:  . .\scripts\env.ps1
# Keeps EVERYTHING inside the project folder (nothing written to C:).
$Root = Split-Path -Parent $PSScriptRoot
$TC   = Join-Path $Root ".toolchain"

# Find Visual Studio (with the C++ tools) instead of assuming a version/path.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$VS = $null
if (Test-Path $vswhere) { $VS = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
if (-not $VS) { throw "Visual Studio with the 'Desktop development with C++' workload was not found." }

New-Item -ItemType Directory -Force "$TC\tmp","$Root\data" | Out-Null
$env:TEMP = "$TC\tmp"; $env:TMP = "$TC\tmp"
$env:PIP_CACHE_DIR = "$TC\pipcache"

# Import the MSVC x64 environment (compiler + Windows SDK) into this session.
$vcvars = "$VS\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
$env:TEMP = "$TC\tmp"; $env:TMP = "$TC\tmp"   # vcvars may reset these

$QtDir = Join-Path $TC "qt\6.8.3\msvc2022_64"
$env:PATH = "$QtDir\bin;$VS\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$VS\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
$env:CMAKE_PREFIX_PATH = $QtDir
$env:KINOZ_ROOT = $Root