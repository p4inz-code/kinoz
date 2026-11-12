# Packs dist\app into dist\payload.zip (what Setup.exe carries). No data folder, no debug files.
#   .\scripts\make-payload.ps1
$ErrorActionPreference = 'Stop'
$Root  = Split-Path -Parent $PSScriptRoot
$app   = Join-Path $Root "dist\app"
$stage = Join-Path $Root ".toolchain\tmp\payload-stage"
$zip   = Join-Path $Root "dist\payload.zip"
if (-not (Test-Path "$app\kinoz.exe")) { throw "dist\app\kinoz.exe not found - build first" }
if (Test-Path $stage) { cmd /c rmdir /s /q "$stage" }
New-Item -ItemType Directory -Force $stage | Out-Null
robocopy $app $stage /E /XD data /XF *.pdb *.ilk unins*.exe unins*.dat /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copy failed ($LASTEXITCODE)" }
# manifest.json: every file with its size and SHA-256 (the setup and the Doctor verify against it)
$ver = ([regex]::Match((Get-Content "$Root\CMakeLists.txt" -Raw), 'project\(\S+\s+VERSION\s+([0-9.]+)')).Groups[1].Value
$base = (Resolve-Path $stage).Path.TrimEnd('\') + '\'
$files = Get-ChildItem $stage -Recurse -File | Where-Object { $_.Name -ne 'manifest.json' } | ForEach-Object {
  [ordered]@{ p = $_.FullName.Substring($base.Length).Replace('\', '/'); s = $_.Length; h = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() }
}
$json = [ordered]@{ version = $ver; files = @($files) } | ConvertTo-Json -Depth 4 -Compress
[IO.File]::WriteAllText("$stage\manifest.json", $json, (New-Object Text.UTF8Encoding($false)))
"manifest: {0} files, version {1}" -f @($files).Count, $ver
if (Test-Path $zip) { Remove-Item $zip -Force }
tar -a -c -f $zip -C $stage .
if ($LASTEXITCODE -ne 0) { throw "tar failed" }
cmd /c rmdir /s /q "$stage"
"payload.zip: {0:N1} MB" -f ((Get-Item $zip).Length / 1MB)
