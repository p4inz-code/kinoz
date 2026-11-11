# Recreates the project-local toolchain (Qt, libmpv, Inno Setup) from nothing.  Run once after cloning:
#     powershell -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1
# Everything is written under .\.toolchain (nothing on C:), and every download is checksum-checked where a checksum exists.
param([switch]$Latest)       # -Latest: use the newest libmpv build instead of the pinned one
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$TC   = Join-Path $Root ".toolchain"
foreach ($d in 'tmp','pylib','qt','mpv','dl','pipcache') { New-Item -ItemType Directory -Force "$TC\$d" | Out-Null }
$env:TEMP = "$TC\tmp"; $env:TMP = "$TC\tmp"; $env:PIP_CACHE_DIR = "$TC\pipcache"

# ---- pinned versions ----
$QtVersion = '6.8.3'
$MpvTag    = '20261111'
$MpvAsset  = 'mpv-dev-x86_64-20261111-git-bcde6b93cc.7z'
$MpvSha256 = '273c079b11ea5575822f972756f4c9a71149a5c10130353abeb8586738938452'

# ---- 1. Qt via aqtinstall (qtbase only) ----
if (-not (Test-Path "$TC\qt\$QtVersion\msvc2022_64\bin\Qt6Core.dll")) {
  Write-Host "Installing Qt $QtVersion ..."
  python -m pip install --no-cache-dir --target "$TC\pylib" aqtinstall
  python -c "import sys; sys.path.insert(0, r'$TC\pylib'); from aqt.installer import Cli; sys.exit(Cli().run(['install-qt','windows','desktop','$QtVersion','win64_msvc2022_64','-O',r'$TC\qt','--archives','qtbase']))"
  if ($LASTEXITCODE -ne 0) { throw "Qt install failed" }
}

# ---- 2. libmpv (dev build) ----
if (-not (Test-Path "$TC\mpv\libmpv-2.dll")) {
  $headers = @{ 'User-Agent' = 'kinoz-setup' }
  $api = if ($Latest) { 'https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest' } else { "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/tags/$MpvTag" }
  $rel = Invoke-RestMethod -Uri $api -Headers $headers
  $asset = if ($Latest) { $rel.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d' } | Select-Object -First 1 } else { $rel.assets | Where-Object { $_.name -eq $MpvAsset } | Select-Object -First 1 }
  if (-not $asset) { throw "libmpv asset not found (try -Latest)" }
  $dest = "$TC\dl\$($asset.name)"
  Write-Host "Downloading $($asset.name) ..."
  Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $dest
  if (-not $Latest) {
    $got = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
    if ($got -ne $MpvSha256) { throw "libmpv checksum mismatch: $got" }
  }
  tar -xf $dest -C "$TC\mpv"          # Windows' bsdtar reads .7z (BCJ2) fine
  if (-not (Test-Path "$TC\mpv\libmpv-2.dll")) { throw "libmpv extraction failed" }
}

# ---- 3. MSVC import library for libmpv ----
if (-not (Test-Path "$TC\mpv\mpv.lib")) {
  . "$PSScriptRoot\env.ps1"
  $exports = dumpbin /exports "$TC\mpv\libmpv-2.dll" | ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(mpv_\w+)') { $Matches[1] } }
  "LIBRARY libmpv-2.dll`r`nEXPORTS`r`n" + ($exports -join "`r`n") | Set-Content -Encoding ascii "$TC\mpv\mpv.def"
  lib /nologo /def:"$TC\mpv\mpv.def" /machine:x64 /out:"$TC\mpv\mpv.lib"
}

# ---- 4. Inno Setup (installer builder), optional ----
if (-not (Get-ChildItem "$TC\innosetup" -Filter ISCC.exe -Recurse -ErrorAction SilentlyContinue)) {
  Write-Host "Installing Inno Setup (winget) ..."
  winget install --id JRSoftware.InnoSetup -e --silent --scope user --location "$TC\innosetup" --accept-package-agreements --accept-source-agreements
}
Write-Host "Toolchain ready. Next:  scripts\build.ps1   (or scripts\release.ps1 for the installer)"