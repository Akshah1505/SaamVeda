# FR4 / SRS-3.6: a recording must survive the application being terminated.
#
# Starts a recording, kills the process outright part-way through - no shutdown,
# no flush - and then reads back whatever reached the disk.
#
#   powershell -File tools\recording-check\crash-test.ps1
#   powershell -File tools\recording-check\crash-test.ps1 -RecordSeconds 40 -KillAfterSeconds 25

param(
    [int]$RecordSeconds = 60,
    [int]$KillAfterSeconds = 20,
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildDir = if ($Config -eq 'Debug') { Join-Path $Root 'build' } else { Join-Path $Root ("build-" + $Config.ToLowerInvariant()) }
$Exe = Join-Path $BuildDir "SaamVedaRecordingCheck_artefacts\$Config\SaamVedaRecordingCheck.exe"

if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Not built yet. Run: tools\build.ps1 build"
}

$recordings = Join-Path ([Environment]::GetFolderPath('MyMusic')) 'SaamVeda Studio\Recordings'
$before = @()
if (Test-Path -LiteralPath $recordings) {
    $before = Get-ChildItem -LiteralPath $recordings -Filter *.wav | ForEach-Object { $_.FullName }
}

$log = Join-Path ([System.IO.Path]::GetTempPath()) 'saamveda-crash-test.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }

Write-Output "Starting a $RecordSeconds second recording..."
$proc = Start-Process -FilePath $Exe -ArgumentList "--record=$RecordSeconds" `
                      -RedirectStandardOutput $log -PassThru -NoNewWindow

# The harness prints this once the transport is actually in record; killing
# before that would test nothing.
$startedAt = $null
for ($i = 0; $i -lt 120; $i++) {
    Start-Sleep -Seconds 1
    if ($proc.HasExited) { throw "Harness exited early. See $log" }
    if ((Test-Path -LiteralPath $log) -and (Select-String -Path $log -Pattern 'record.started = yes' -Quiet)) {
        $startedAt = Get-Date
        break
    }
}

if (-not $startedAt) { throw "Recording never started. See $log" }

Write-Output "Recording. Killing the process in $KillAfterSeconds seconds."
Start-Sleep -Seconds $KillAfterSeconds

# Stop-Process -Force is a TerminateProcess: no unwinding, no destructors, no
# chance for the engine to close the file. That is the point.
Stop-Process -Id $proc.Id -Force
$killedAfter = ((Get-Date) - $startedAt).TotalSeconds
Write-Output ("Killed after {0:N1} s of recording." -f $killedAfter)

Start-Sleep -Seconds 2

$after = Get-ChildItem -LiteralPath $recordings -Filter *.wav | Where-Object { $before -notcontains $_.FullName }

if (-not $after) {
    Write-Output "crash.newFile = none"
    Write-Output "crash.result = FAIL"
    exit 1
}

$take = ($after | Sort-Object LastWriteTime -Descending)[0]
Write-Output "crash.file = $($take.FullName)"
Write-Output "crash.bytes = $($take.Length)"
Write-Output ("crash.expectedSeconds = {0:N1}" -f $killedAfter)

# What is reachable before anything repairs it. The last few seconds of audio
# are on disk but the data chunk header has not caught up with them.
Write-Output "--- as the crash left it ---"
& $Exe --analyse $take.FullName

# The same pass the application runs at startup.
Write-Output "--- after recovery ---"
& $Exe --recover
& $Exe --analyse $take.FullName
exit $LASTEXITCODE
