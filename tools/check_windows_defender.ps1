# Explicit-file release check. Never changes exclusions, turns off protection,
# restores quarantine or executes scanned files. No-remediation custom scans
# ignore exclusions, inspect archives and report detections in command output.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string[]]$Files,
    [string]$ReportPath = 'build/validation/active/windows-defender-report.json',
    [switch]$UpdateSignatures
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'windows_defender_verdict.ps1')
$targets = @($Files | ForEach-Object {
    $item = Get-Item -LiteralPath $_ -ErrorAction Stop
    if ($item.PSIsContainer) { throw 'Only explicit files may be scanned' }
    $item.FullName # MpCmdRun requires native Windows path separators.
})
$status = Get-MpComputerStatus -ErrorAction Stop
if (!$status.AMServiceEnabled -or !$status.AntivirusEnabled -or !$status.RealTimeProtectionEnabled) {
    throw 'Defender protection is unavailable; release scan cannot pass'
}
if ($UpdateSignatures) {
    Update-MpSignature -ErrorAction Stop
    $status = Get-MpComputerStatus -ErrorAction Stop
}
$platform = Join-Path $env:ProgramData "Microsoft/Windows Defender/Platform/$($status.AMProductVersion)-0/MpCmdRun.exe"
$scanner = if (Test-Path -LiteralPath $platform -PathType Leaf) { $platform }
    else { Join-Path $env:ProgramFiles 'Windows Defender/MpCmdRun.exe' }
if (!(Test-Path -LiteralPath $scanner -PathType Leaf)) { throw 'Defender scanner not installed' }
$results = @()
foreach ($target in $targets) {
    $before = Get-FileHash -LiteralPath $target -Algorithm SHA256
    $bytes = (Get-Item -LiteralPath $target).Length
    $scanOutput = (& $scanner -Scan -ScanType 3 -File $target -DisableRemediation 2>&1 | Out-String)
    $scanCode = $LASTEXITCODE
    $present = Test-Path -LiteralPath $target -PathType Leaf
    $afterHash = if ($present) { (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash } else { '' }
    $passed = Test-WindowsDefenderResult $scanCode $scanOutput $present $before.Hash $afterHash
    $results += [pscustomobject]@{name=[IO.Path]::GetFileName($target); bytes=$bytes;
        sha256=$before.Hash.ToLowerInvariant(); exit_code=$scanCode; passed=$passed;
        scan_output=$scanOutput.Trim()}
    Write-Output "$([IO.Path]::GetFileName($target)): Defender scan passed=$passed, exit=$scanCode"
}
$passed = @($results | Where-Object { !$_.passed }).Count -eq 0
$report = [ordered]@{schema=1; scanned_utc=[DateTime]::UtcNow.ToString('o');
    defender_engine=$status.AMEngineVersion; defender_signatures=$status.AntivirusSignatureVersion;
    signature_updated_utc=$status.AntivirusSignatureLastUpdated.ToUniversalTime().ToString('o');
    scope='Explicit file/archive scan only; not a SmartScreen, cloud-reputation or all-vendor verdict';
    passed=$passed; files=$results}
$reportFile = [IO.Path]::GetFullPath($ReportPath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reportFile) | Out-Null
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportFile -Encoding utf8
if (!$passed) { throw "Defender detection, scan error or changed/missing input; release blocked. See $reportFile" }
