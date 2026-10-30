# UNTESTED: native Windows build with MSYS2 (MINGW64 shell tools on PATH: cmake, ninja, g++, git, curl, sha256sum, bash).
# The shipped binaries were cross-built on Linux (scripts/build.sh); this route has never been run.
# shortcut: only builds, no test run or packaging; run scripts/build.sh inside the MSYS2 shell for the full pipeline.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot)
bash scripts/fetch_deps.sh
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
Write-Host "Built into build-win\stage. Run build-win\BodySyncTests.exe and build-win\DriverHarness.exe to test."
