$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/windows_defender_verdict.ps1')
$clean = 'Scan starting... Scan finished. Scanning fixture.exe found no threats.'
$hash = 'A' * 64
if (!(Test-WindowsDefenderResult 0 $clean $true $hash $hash)) { throw 'Clean stable scan rejected' }
foreach ($bad in @(
    @(2, $clean, $true, $hash, $hash),
    @(0, '', $true, $hash, $hash),
    @(0, 'Scan finished. Threat Name: Trojan:Test', $true, $hash, $hash),
    @(0, "$clean CmdTool: Failed", $true, $hash, $hash),
    @(0, $clean, $false, $hash, $hash),
    @(0, $clean, $true, $hash, ('B' * 64)),
    @(0, $clean, $true, '', ''))) {
    if (Test-WindowsDefenderResult @bad) { throw 'Failed/ambiguous/changed scan accepted' }
}
Write-Output 'Defender verdict: clean success, detections, errors, missing output/file and changed hash checks passed.'
