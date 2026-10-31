# Package download instructions only. Optional third-party system installers
# are not game dependencies and must not be redistributed in the game ZIP.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (!(Test-Path -LiteralPath $output -PathType Container)) {
    throw 'Game package directory does not exist'
}
$manifestPath = Join-Path $PSScriptRoot 'package/displayxr-runtime.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1) { throw 'Unsupported DisplayXR reference manifest' }

# Fail on stale payloads instead of silently deleting files from a caller's
# directory. A fresh package is required if an old build bundled installers.
foreach ($file in Get-ChildItem -LiteralPath $output -Recurse -File -Force) {
    if ($file.Name -match '^DisplayXR.*\.(exe|msi|msix|msixbundle)$') {
        throw "Third-party DisplayXR installer is not permitted in the game package: $($file.Name)"
    }
}
$destination = Join-Path $output 'optional-runtimes/displayxr'
if (!$VerifyOnly) {
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
}
foreach ($pair in @(
    @((Join-Path $PSScriptRoot 'package/DISPLAYXR-INSTALLERS.txt'), (Join-Path $destination 'README.txt')),
    @($manifestPath, (Join-Path $destination 'manifest.json')))) {
    if (!$VerifyOnly) { Copy-Item -LiteralPath $pair[0] -Destination $pair[1] -Force }
    if (!(Test-Path -LiteralPath $pair[1] -PathType Leaf) -or
        (Get-FileHash -LiteralPath $pair[0]).Hash -ne (Get-FileHash -LiteralPath $pair[1]).Hash) {
        throw 'DisplayXR download instructions/reference manifest missing or modified'
    }
}
$allowed = @((Join-Path $destination 'README.txt'), (Join-Path $destination 'manifest.json'))
foreach ($file in Get-ChildItem -LiteralPath $destination -Recurse -File -Force) {
    if ($file.FullName -notin $allowed) {
        throw "Unexpected optional runtime payload: $($file.Name)"
    }
}
Write-Output 'DisplayXR package verified: download links only; no installer downloaded, bundled, executed or installed.'
