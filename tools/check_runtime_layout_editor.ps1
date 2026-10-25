param(
    [string]$Binary='build/current/starfox_pc.exe',
    [string]$OutputDirectory='build/validation/active/runtime-layout-editor'
)
$ErrorActionPreference='Stop'
$binaryPath=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
$active=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build/validation/active'))
if(!$output.StartsWith($active+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a new directory inside build/validation/active'
}
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory; previous results are not overwritten'}
& (Join-Path $PSScriptRoot '../build/validation/check-storage.ps1') -PlannedBytes 1048576
if($LASTEXITCODE) {throw 'Storage guard failed'}
New-Item -ItemType Directory -Path $output | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_RENDER_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    foreach($experience in 'ORIGINAL','EX') {
        $directory=Join-Path $output $experience
        New-Item -ItemType Directory -Path $directory | Out-Null
        $settings=@{
            STARFOX_TEST_FRAMES='165';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_UNPACED='1';
            STARFOX_TEST_FIXED_RASTER='1';STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='80';
            STARFOX_TEST_EXPERIENCE=$experience;STARFOX_TEST_RENDERER='SOFTWARE';
            STARFOX_TEST_DATA_DIRECTORY=$directory;STARFOX_TEST_WINDOW_SIZE='400x224';
            STARFOX_TEST_PRESENTATION_FPS='60';STARFOX_TEST_DISPLAY_MODE='16_9';
            STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_TIMING_MODE='UNLOCKED';
            STARFOX_TEST_VSYNC='0';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';
            STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0';
            STARFOX_TEST_STATE_ACTIONS='12:6,152:6';
            STARFOX_TEST_PRESSES='24:128,36:1024,48:1024,60:1024,72:128,96:32768,112:128,136:128';
            STARFOX_TEST_PRESS_FRAMES='3';SDL_AUDIODRIVER='dummy';SDL_RENDER_DRIVER='software'
        }
        foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
        $log=Join-Path $directory 'runtime.log'
        $process=Start-Process -FilePath $binaryPath -ArgumentList 'LEVEL1_2' -WindowStyle Hidden -PassThru -RedirectStandardError $log
        while(!$process.WaitForExit(10000)) {Write-Output "Runtime editor check active: $experience, PID $($process.Id)"}
        if($process.ExitCode -ne 0) {throw "Runtime editor failed: $experience ($($process.ExitCode))"}
        $text=Get-Content -LiteralPath $log -Raw
        $rows=[regex]::Matches($text,'runtime-layout-editor: HUD (open|cancel|apply) map=(\d+) objects=(\d+) scene=(\d+)')
        if($rows.Count -ne 4) {throw "Expected open/cancel/open/apply: $experience ($($rows.Count) events), $log"}
        if(($rows | ForEach-Object {$_.Groups[1].Value}) -join ',' -ne 'open,cancel,open,apply') {
            throw 'Incorrect editor transition order'
        }
        for($i=0;$i -lt 4;$i+=2) {
            foreach($group in 2,3,4) {
                if($rows[$i].Groups[$group].Value -ne $rows[$i+1].Groups[$group].Value) {
                    throw "Paused cartridge/object/scene state changed: $experience, editor pair $i"
                }
            }
        }
        if(([regex]::Matches($text,'runtime-options open=1')).Count -ne 1 -or
            ([regex]::Matches($text,'runtime-options open=0')).Count -ne 1) {
            throw 'F1 did not resume the same runtime options session'
        }
        if($text -match 'preview: RENDERING|preview: rendered scene ready') {throw 'Editor restarted a preview'}
        if(!(Test-Path -LiteralPath (Join-Path $directory 'hud-layout.cfg'))) {throw 'Apply did not save the isolated layout'}
        Write-Output "PASS: $experience F1 -> HUD edit -> cancel -> edit -> apply -> F1 resume; exact paused map/object hashes and scene revision preserved"
        foreach($key in $settings.Keys) {Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue}
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_RENDER_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
