param(
    [string]$OutputDirectory='build/validation/active/background-routes',
    [ValidateRange(1,8000)][int]$Ticks=8000,
    [string]$Executable='build/current/starfox_background_transition_check.exe',
    [switch]$Movement
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $root $OutputDirectory}))
$allowed=[IO.Path]::GetFullPath((Join-Path $root 'build/validation/active'))+[IO.Path]::DirectorySeparatorChar
if(!$output.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Route audit output must be inside build/validation/active'
}
if(Test-Path -LiteralPath $output) {throw 'Choose a new audit directory; existing receipts will not be replaced'}
$validation=Join-Path $root 'build/validation'
if([IO.Path]::GetPathRoot($output) -ine 'C:\') {throw 'Validation scratch must stay on C:'}
$used=[long]0
if(Test-Path -LiteralPath $validation) {
    foreach($file in Get-ChildItem -LiteralPath $validation -File -Recurse -Force) {$used+=$file.Length}
}
if($used+4000000 -gt 1GB -or (Get-PSDrive C).Free-4000000 -lt 20GB) {
    throw 'Route audit would exceed the 1 GiB scratch budget or 20 GiB free-space reserve'
}
New-Item -ItemType Directory -Path $output | Out-Null
$records=@()
foreach($experience in @('ORIGINAL','EX')) {
    $rom=if($experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
    $symbols=if($experience -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
    $levels=@(Get-Content -LiteralPath (Join-Path $root $symbols) | ForEach-Object {
        if($_ -match '^(LEVEL(?:[1-7]_[1-9]|_BLACKHOLE|_SPECIAL|_COMET))\s') {$Matches[1]}
    } | Sort-Object -Unique)
    foreach($level in $levels) {
        $mode=if($Movement){'--audit-moving'}else{'--audit'}
        $lines=@(& (Join-Path $root $Executable) (Join-Path $root $rom) (Join-Path $root $symbols) $level $Ticks $mode 2>&1)
        $exitCode=$LASTEXITCODE
        $lines | Set-Content -LiteralPath (Join-Path $output "$experience-$level.txt")
        $records+= [pscustomobject]@{experience=$experience;level=$level;ticks=$Ticks;exit_code=$exitCode;
            backgrounds=@($lines | Where-Object {$_ -match '^audit_span '});
            error=if($exitCode){($lines | Select-Object -Last 3)-join "`n"}else{$null}}
        Write-Output "$experience $level exit=$exitCode ($($records.Count) entries audited)"
    }
}
[pscustomobject]@{schema=1;input=$(if($Movement){'bank-pitch-laser'}else{'none'});god_mode=$true;audio='real SPC, synchronized ports';
    limitations='Bounded direct-entry paths, not completed routes or all branches. Transform sweeps are separate from gameplay coverage.';
    records=$records} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'coverage.json')
if(@($records | Where-Object exit_code -ne 0).Count) {throw 'Some entries failed; inspect coverage.json'}
