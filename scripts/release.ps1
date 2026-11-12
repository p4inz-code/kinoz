# Build the app and produce the single-file installer:  .\scripts\release.ps1
# Uses at most 4 cores and nothing outside this folder.
. "$PSScriptRoot\env.ps1"
& "$PSScriptRoot\build.ps1"
if ($LASTEXITCODE -ne 0) { throw "build failed" }
& "$PSScriptRoot\make-payload.ps1"
$iscc = Get-ChildItem "$TC\innosetup" -Filter ISCC.exe -Recurse | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup not found in .toolchain\innosetup (run scripts\setup-toolchain.ps1)" }
& $iscc.FullName "$Root\packaging\installer.iss"
$inst = Get-ChildItem "$Root\dist\installer\kinoz-Setup-*.exe" | Select-Object -First 1
(Get-FileHash $inst.FullName -Algorithm SHA256).Hash.ToLower() + "  " + $inst.Name | Set-Content "$($inst.FullName).sha256"
Get-ChildItem "$Root\dist\installer" | Select-Object Name, @{n='MB';e={[math]::Round($_.Length/1MB,1)}}
