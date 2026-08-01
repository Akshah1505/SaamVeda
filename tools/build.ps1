# Wrapper so build commands run with the pinned toolchain without needing an
# interactive dev shell. Usage:
#   powershell -File tools\build.ps1 configure
#   powershell -File tools\build.ps1 build
#   powershell -File tools\build.ps1 test

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('configure', 'build', 'test', 'clean')]
    [string]$Task,

    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

$Root     = Split-Path -Parent $PSScriptRoot
$CMakeBin = 'D:\Tools\cmake-3.31.12-windows-x86_64\bin'
$NinjaBin = 'D:\Tools\ninja-1.13.2'
$VcVars   = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'

cmd /c "call `"$VcVars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
    }
}
$env:PATH = "$CMakeBin;$NinjaBin;$env:PATH"

$BuildDir = if ($Config -eq 'Debug') {
    Join-Path $Root 'build'
} else {
    Join-Path $Root ("build-" + $Config.ToLowerInvariant())
}

switch ($Task) {
    'configure' {
        & cmake -S $Root -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
    }
    'build' {
        & cmake --build $BuildDir --parallel
    }
    'test' {
        & ctest --test-dir $BuildDir --output-on-failure
    }
    'clean' {
        if (Test-Path -LiteralPath $BuildDir) { Remove-Item -LiteralPath $BuildDir -Recurse -Force }
    }
}

exit $LASTEXITCODE
