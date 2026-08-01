# SaamVeda Studio - development shell
#
# Puts the pinned toolchain on PATH for THIS SESSION ONLY and initialises the
# MSVC environment. Nothing global is modified.
#
# Usage:
#   powershell -NoExit -File tools\devshell.ps1
#
# See docs/07-tech-stack.md section 5 for the version pinning policy.

$ErrorActionPreference = 'Stop'

$CMakeBin = 'D:\Tools\cmake-3.31.12-windows-x86_64\bin'
$NinjaBin = 'D:\Tools\ninja-1.13.2'
$VcVars   = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'

foreach ($p in @($CMakeBin, $NinjaBin)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Toolchain path missing: $p" }
}
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found: $VcVars" }

# Import the MSVC environment by running vcvars64.bat and capturing the result.
# vcvars sets INCLUDE/LIB/PATH which the compiler and linker require.
cmd /c "call `"$VcVars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
    }
}

$env:PATH = "$CMakeBin;$NinjaBin;$env:PATH"

Write-Host ''
Write-Host 'SaamVeda Studio dev shell' -ForegroundColor Cyan
Write-Host ('  cmake  ' + ((cmake --version) -split "`n")[0])
Write-Host ('  ninja  ' + (ninja --version))
Write-Host ('  cl     ' + (( & cmd /c 'cl 2>&1' ) | Select-Object -First 1))
Write-Host ''
Write-Host 'Configure:  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug'
Write-Host 'Build:      cmake --build build --parallel'
Write-Host 'Test:       ctest --test-dir build --output-on-failure'
Write-Host ''
