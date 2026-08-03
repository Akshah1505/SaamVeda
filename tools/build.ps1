# Wrapper so build commands run with the project toolchain without needing an
# interactive dev shell. Usage:
#   powershell -File tools\build.ps1 configure
#   powershell -File tools\build.ps1 build
#   powershell -File tools\build.ps1 test
#   powershell -File tools\build.ps1 run
#
# Toolchain locations are discovered by tools\toolchain.ps1; override with
# SAAMVEDA_CMAKE / SAAMVEDA_NINJA / SAAMVEDA_VCVARS if needed.

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('configure', 'build', 'test', 'run', 'clean')]
    [string]$Task,

    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Initialize-Toolchain | Out-Null

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
        if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))) {
            & cmake -S $Root -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        & cmake --build $BuildDir --parallel
    }
    'test' {
        & ctest --test-dir $BuildDir --output-on-failure
    }
    'run' {
        $exe = Join-Path $BuildDir 'SaamVedaStudio_artefacts\SaamVeda Studio.exe'
        if (-not (Test-Path -LiteralPath $exe)) {
            $exe = Join-Path $BuildDir "SaamVedaStudio_artefacts\$Config\SaamVeda Studio.exe"
        }
        if (-not (Test-Path -LiteralPath $exe)) {
            throw "Application not built yet. Run: tools\build.ps1 build"
        }
        & $exe
    }
    'clean' {
        if (Test-Path -LiteralPath $BuildDir) { Remove-Item -LiteralPath $BuildDir -Recurse -Force }
    }
}

exit $LASTEXITCODE
