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

$BuildDir = if ($Config -eq 'Debug') {
    Join-Path $Root 'build'
} else {
    Join-Path $Root ("build-" + $Config.ToLowerInvariant())
}

# CMake pins the compiler in its cache at configure time, but vcvars is picked
# fresh on every build. Installing a newer Visual Studio therefore puts its STL
# headers in front of the older cached cl.exe, and the build dies on
# "STL1001: Unexpected compiler version". Pin vcvars to whichever Visual Studio
# owns the compiler this build directory was configured with.
function Get-ConfiguredVcVars {
    param([string]$Directory)

    $cache = Join-Path $Directory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache)) { return $null }

    $line = Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=(.+)$' |
        Select-Object -First 1
    if (-not $line) { return $null }

    # Normalised to forward slashes so the pattern needs no backslash escaping.
    $compiler = $line.Matches[0].Groups[1].Value.Replace([char]92, '/')
    if ($compiler -notmatch '^(.*?)/VC/Tools/MSVC/') { return $null }

    $candidate = (Join-Path $matches[1] 'VC/Auxiliary/Build/vcvars64.bat').Replace('/', [char]92)
    if (Test-Path -LiteralPath $candidate) { return $candidate }

    return $null
}

if (-not $env:SAAMVEDA_VCVARS) {
    $configured = Get-ConfiguredVcVars -Directory $BuildDir
    if ($configured) { $env:SAAMVEDA_VCVARS = $configured }
}

Initialize-Toolchain | Out-Null

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
