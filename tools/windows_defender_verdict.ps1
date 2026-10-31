# Pure fail-closed predicate, shared by the release scanner and offline tests.
function Test-WindowsDefenderResult {
    param([int]$ExitCode, [string]$Output, [bool]$FilePresent,
        [string]$BeforeHash, [string]$AfterHash)
    return $ExitCode -eq 0 -and $FilePresent -and
        $BeforeHash -match '^[0-9A-Fa-f]{64}$' -and $BeforeHash -eq $AfterHash -and
        $Output -match '(?i)Scan finished\.' -and
        $Output -match '(?i)found no threats\.' -and
        $Output -notmatch '(?i)CmdTool: Failed|Threat(?: Name)?\s*:|Trojan:|PUA:'
}
