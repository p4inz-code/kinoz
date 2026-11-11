# Downloads the two open-licence (OFL) fonts used by your other apps into resources/fonts.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $Root "resources\fonts"
New-Item -ItemType Directory -Force $out | Out-Null

function Get-Dir($api) { Invoke-RestMethod -Uri $api -Headers @{ 'User-Agent' = 'font-fetch' } -TimeoutSec 30 }

$jobs = @(
  @{ api = 'https://api.github.com/repos/googlefonts/atkinson-hyperlegible-next/contents/fonts/ttf'; pick = 'Regular|Medium|SemiBold|Bold' },
  @{ api = 'https://api.github.com/repos/google/fonts/contents/ofl/familjengrotesk'; pick = '\.ttf$' }
)
foreach ($j in $jobs) {
  try {
    foreach ($f in (Get-Dir $j.api)) {
      if ($f.type -eq 'file' -and $f.name -match '\.ttf$' -and $f.name -match $j.pick -and $f.name -notmatch 'Italic') {
        $dest = Join-Path $out ($f.name -replace '[\[\]]','')
        Invoke-WebRequest -Uri $f.download_url -OutFile $dest -TimeoutSec 120
        "got $($f.name)"
      }
    }
  } catch { "FAILED $($j.api): $($_.Exception.Message)" }
}
Get-ChildItem $out -Filter *.ttf | Select-Object Name, Length
