# Native Windows build with MSYS2 (MINGW64 shell tools on PATH: cmake, ninja, g++, git, curl, sha256sum, bash), see BUILD.md.
# Builds only; run scripts/build.sh inside the MSYS2 shell for the full pipeline with tests and packaging.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot)
bash scripts/fetch_deps.sh
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
Write-Host "Built into build-win\stage. Run build-win\BodySyncTests.exe and build-win\DriverHarness.exe to test."
