# SaamVeda Studio - development shell
#
# Puts the project toolchain on PATH for THIS SESSION ONLY and initialises the
# MSVC environment. Nothing global is modified.
#
# Usage:
#   powershell -NoExit -File tools\devshell.ps1
#
# See docs/07-tech-stack.md section 5 for the version pinning policy and
# tools\toolchain.ps1 for how the tools are located.

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Initialize-Toolchain

Write-Host ''
Write-Host 'SaamVeda Studio dev shell' -ForegroundColor Cyan
Write-Host ('  cmake  ' + ((cmake --version) -split "`n")[0] + "  [$($toolchain.CMakeBin)]")
Write-Host ('  ninja  ' + (ninja --version) + "  [$($toolchain.NinjaBin)]")
Write-Host ('  msvc   ' + $toolchain.VcVars)
Write-Host ''
Write-Host 'Configure:  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug'
Write-Host 'Build:      cmake --build build --parallel'
Write-Host 'Test:       ctest --test-dir build --output-on-failure'
Write-Host ''
