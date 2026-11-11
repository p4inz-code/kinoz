# Build the app and produce the installer:  .\scripts\release.ps1
# Uses at most 4 cores and nothing outside this folder.
. "$PSScriptRoot\env.ps1"
& "$PSScriptRoot\build.ps1"
if ($LASTEXITCODE -ne 0) { throw "build failed" }
$iscc = Get-ChildItem "$TC\innosetup" -Filter ISCC.exe -Recurse | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup not found in .toolchain\innosetup (winget install JRSoftware.InnoSetup --location ...)" }
& $iscc.FullName "$Root\packaging\installer.iss"
Get-ChildItem "$Root\dist\installer" | Select-Object Name, @{n='MB';e={[math]::Round($_.Length/1MB,1)}}
