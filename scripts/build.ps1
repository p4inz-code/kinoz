# Configure + build (Release) entirely inside the project folder.  Usage: .\scripts\build.ps1 [-Clean]
param([switch]$Clean)
. "$PSScriptRoot\env.ps1"
$build = Join-Path $Root "build"
if ($Clean -and (Test-Path $build)) { Remove-Item $build -Recurse -Force }
if (-not (Test-Path "$build\build.ninja")) {
    cmake -S $Root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir" 2>&1 | Select-Object -Last 8
}
cmake --build $build -j 4 2>&1
exit $LASTEXITCODE

