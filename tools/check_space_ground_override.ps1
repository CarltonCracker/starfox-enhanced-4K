param(
    [string]$Binary='build/current/starfox_pc.exe',
    [string]$OutputDirectory='build/validation/active/space-ground-override',
    [switch]$ExpectRegression
)
$ErrorActionPreference='Stop'
$binaryPath=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
$active=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/validation/active'))
if(!$output.StartsWith($active+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Use build/validation/active'}
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory'}
& (Join-Path $PSScriptRoot '../build/validation/check-storage.ps1') -PlannedBytes 4194304
if($LASTEXITCODE) {throw 'Storage guard failed'}
New-Item -ItemType Directory -Path $output | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_RENDER_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    foreach($case in @(
        @{name='venom';experience='ORIGINAL';stage='LEVEL1_5';background='BG_1_5';ground=$false},
        @{name='orbital';experience='ORIGINAL';stage='LEVEL2_2';background='BG_2_2';ground=$false},
        @{name='ex-orbital';experience='EX';stage='LEVEL2_2';background='BG_2_2';ground=$false},
        @{name='ex-lava';experience='EX';stage='LEVEL6_6';background='BG_6_6';ground=$true}
    )) {
        $hashes=@()
        foreach($enabled in 0,1) {
            $directory=Join-Path $output "$($case.name)-$enabled"
            New-Item -ItemType Directory -Path $directory | Out-Null
            $settings=@{
                STARFOX_TEST_FRAMES='1';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_UNPACED='1';
                STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000';
                STARFOX_TEST_EXPERIENCE=$case.experience;STARFOX_TEST_RENDERER='SOFTWARE';
                STARFOX_TEST_BACKGROUND=$case.background;STARFOX_TEST_DATA_DIRECTORY=$directory;
                STARFOX_TEST_WINDOW_SIZE='400x224';STARFOX_TEST_DISPLAY_MODE='16_9';
                STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_PRESENTATION_FPS='60';
                STARFOX_TEST_VSYNC='0';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';
                STARFOX_TEST_ENVIRONMENT_0="$enabled";STARFOX_TEST_ENVIRONMENT_1='9';STARFOX_TEST_ENVIRONMENT_3='0';
                STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0';
                STARFOX_TEST_SHOW_FPS='0';STARFOX_TEST_GOD_MODE='1';
                STARFOX_TRACE_GPU='1';SDL_AUDIODRIVER='dummy';SDL_RENDER_DRIVER='software';
                STARFOX_CAPTURE_PATH=(Join-Path $directory 'frame.bmp')
            }
            foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
            $process=Start-Process -FilePath $binaryPath -ArgumentList $case.stage -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $directory 'runtime.log')
            while(!$process.WaitForExit(10000)) {Write-Output "Space ground check active: $($case.name), PID $($process.Id)"}
            if($process.ExitCode -ne 0) {throw "Failed space fixture $($case.name): $($process.ExitCode)"}
            $hashes+=(Get-FileHash -LiteralPath $settings.STARFOX_CAPTURE_PATH -Algorithm SHA256).Hash
            foreach($key in $settings.Keys) {Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue}
        }
        $changed=$hashes[0] -ne $hashes[1]
        $expected=$case.ground -or [bool]$ExpectRegression
        if($changed -ne $expected) {throw "Unexpected ground ownership $($case.name): changed=$changed, expected=$expected"}
        Write-Output "PASS: $($case.name), manual lava changes image=$changed, physical ground=$($case.ground)"
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_RENDER_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
