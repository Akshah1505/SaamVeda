# Shared toolchain discovery, dot-sourced by build.ps1 and devshell.ps1.
#
# docs/07-tech-stack.md pins tool versions, but the pinned install *location* is
# a property of one machine, not of the project. Hardcoding it meant the scripts
# broke the moment the tools moved. Preference order:
#
#   1. $env:SAAMVEDA_CMAKE / $env:SAAMVEDA_NINJA / $env:SAAMVEDA_VCVARS
#   2. The pinned paths below, if they still exist
#   3. Whatever is already on PATH
#   4. Well-known install locations (vswhere for MSVC)

$script:PinnedCMakeBin = 'D:\Tools\cmake-3.31.12-windows-x86_64\bin'
$script:PinnedNinjaBin = 'D:\Tools\ninja-1.13.2'

function Find-ToolDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [string]$Override,
        [string]$PinnedDirectory,
        [string[]]$Candidates = @()
    )

    if ($Override) {
        if (Test-Path -LiteralPath $Override -PathType Leaf) { return (Split-Path -Parent $Override) }
        if (Test-Path -LiteralPath $Override -PathType Container) { return $Override }
        throw "Override path for $Executable does not exist: $Override"
    }

    if ($PinnedDirectory -and (Test-Path -LiteralPath (Join-Path $PinnedDirectory "$Executable.exe"))) {
        return $PinnedDirectory
    }

    $onPath = Get-Command $Executable -ErrorAction SilentlyContinue
    if ($onPath) { return (Split-Path -Parent $onPath.Source) }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "$Executable.exe")) { return $candidate }
    }

    return $null
}

function Find-VcVars {
    param([string]$Override)

    if ($Override) {
        if (-not (Test-Path -LiteralPath $Override)) { throw "SAAMVEDA_VCVARS does not exist: $Override" }
        return $Override
    }

    # vswhere ships with every Visual Studio 2017+ installer and is the
    # supported way to locate an install without guessing edition names.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $root = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null | Select-Object -First 1

        if ($root) {
            $candidate = Join-Path $root 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }

    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        foreach ($edition in @('BuildTools', 'Community', 'Professional', 'Enterprise')) {
            $candidate = Join-Path $base "Microsoft Visual Studio\2022\$edition\VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }

    return $null
}

function Initialize-Toolchain {
    $cmakeBin = Find-ToolDirectory -Executable 'cmake' -Override $env:SAAMVEDA_CMAKE `
        -PinnedDirectory $script:PinnedCMakeBin `
        -Candidates @("$env:ProgramFiles\CMake\bin", "${env:ProgramFiles(x86)}\CMake\bin")

    $ninjaBin = Find-ToolDirectory -Executable 'ninja' -Override $env:SAAMVEDA_NINJA `
        -PinnedDirectory $script:PinnedNinjaBin `
        -Candidates @("$env:LOCALAPPDATA\Microsoft\WinGet\Links")

    $vcvars = Find-VcVars -Override $env:SAAMVEDA_VCVARS

    $missing = @()
    if (-not $cmakeBin) { $missing += 'cmake (set SAAMVEDA_CMAKE or install CMake 3.22+)' }
    if (-not $ninjaBin) { $missing += 'ninja (set SAAMVEDA_NINJA or install Ninja)' }
    if (-not $vcvars)   { $missing += 'vcvars64.bat (set SAAMVEDA_VCVARS or install VS 2022 Desktop C++)' }

    if ($missing.Count -gt 0) {
        throw ("Toolchain not found:`n  - " + ($missing -join "`n  - ") + "`nSee docs/08-toolchain-setup.md.")
    }

    # vcvars sets INCLUDE/LIB/PATH, which the compiler and linker require.
    cmd /c "call `"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
        }
    }

    $env:PATH = "$cmakeBin;$ninjaBin;$env:PATH"

    return [pscustomobject]@{
        CMakeBin = $cmakeBin
        NinjaBin = $ninjaBin
        VcVars   = $vcvars
    }
}
