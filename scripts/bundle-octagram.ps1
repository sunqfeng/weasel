param(
  [string]$DataDirectory = (Join-Path $PSScriptRoot '..\output\data')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$masterCommit = '8ceef1b42eb77e86501382a52e85c309c0f2f04c'
$hansCommit = 'f8ce3b534733e489a8470a7c2adf5a154e8ea069'
$repository = 'https://raw.githubusercontent.com/lotem/rime-octagram-data'

function Install-PinnedFile {
  param(
    [Parameter(Mandatory)] [string]$Url,
    [Parameter(Mandatory)] [string]$Destination,
    [Parameter(Mandatory)] [string]$Sha256
  )

  if (Test-Path -LiteralPath $Destination) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash
    if ($actual -eq $Sha256) {
      Write-Host "Using cached $(Split-Path -Leaf $Destination)"
      return
    }
  }

  $temporary = "$Destination.download"
  try {
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $temporary
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $temporary).Hash
    if ($actual -ne $Sha256) {
      throw "Checksum mismatch for $Url. Expected $Sha256, got $actual."
    }
    Move-Item -Force -LiteralPath $temporary -Destination $Destination
  } finally {
    Remove-Item -Force -LiteralPath $temporary -ErrorAction SilentlyContinue
  }
}

New-Item -ItemType Directory -Force -Path $DataDirectory | Out-Null

Install-PinnedFile `
  -Url "$repository/$masterCommit/grammar.yaml" `
  -Destination (Join-Path $DataDirectory 'grammar.yaml') `
  -Sha256 'A58CE9EF1DC8106634ED305565A832C06DDEC9C01796501B1A4ADAEB77264129'

Install-PinnedFile `
  -Url "$repository/$hansCommit/zh-hans-t-essay-bgw.gram" `
  -Destination (Join-Path $DataDirectory 'zh-hans-t-essay-bgw.gram') `
  -Sha256 'D3CB2438C1FDCD6A855DD6CA8F5C1060A29273C6B64C2C2C69AF67CD71B6AA7E'

Install-PinnedFile `
  -Url "$repository/$masterCommit/LICENSE" `
  -Destination (Join-Path $DataDirectory 'octagram-data.LICENSE.txt') `
  -Sha256 'DA7EABB7BAFDF7D3AE5E9F223AA5BDC1EECE45AC569DC21B3B037520B4464768'

$customization = @'
# Enable the bundled local Octagram model for Luna Pinyin Simplified Chinese.
patch:
  __include: grammar:/hans
'@
$customizationPath = Join-Path $DataDirectory 'luna_pinyin_simp.custom.yaml'
Set-Content -LiteralPath $customizationPath -Value $customization -Encoding utf8NoBOM

$required = @(
  (Join-Path $DataDirectory 'grammar.yaml'),
  (Join-Path $DataDirectory 'zh-hans-t-essay-bgw.gram'),
  $customizationPath,
  (Join-Path $DataDirectory 'octagram-data.LICENSE.txt')
)
foreach ($path in $required) {
  if (-not (Test-Path -LiteralPath $path)) {
    throw "Missing bundled Octagram file: $path"
  }
}

$custom = Get-Content -Raw -LiteralPath $customizationPath
if ($custom -notmatch 'grammar:/hans') {
  throw 'luna_pinyin_simp.custom.yaml does not enable grammar:/hans'
}

Write-Host "Bundled pinned Octagram model in $DataDirectory"
