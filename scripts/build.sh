#!/usr/bin/env bash
# Full pipeline on Linux: verify dependencies and localisation, build (native core + Windows x64 cross build), run every test.
#   scripts/build.sh              build + tests
#   scripts/build.sh --package    additionally create dist/ (portable ZIP, source ZIP, installer) and test the installer
# Raw logs are written to build-results/ (not part of the source ZIP). Exit code 0 only if everything passed.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PACKAGE=0; [ "${1:-}" = "--package" ] && PACKAGE=1
R=build-results; rm -rf "$R"; mkdir -p "$R"
. scripts/wine-env.sh
step(){ echo; echo "=== $*"; }
declare -A RES

step "dependencies (pinned, SHA-256)"; scripts/fetch_deps.sh 2>&1 | tail -1 | tee $R/deps.txt
step "localisation files (en/ru/de/fr/es)"; python3 scripts/check_i18n.py > $R/i18n.txt 2>&1; RES[i18n]=$?; tail -3 $R/i18n.txt
step "native build + portable core tests"; cmake/native-tests.sh build-native > $R/native.txt 2>&1; RES[native]=$?; tail -2 $R/native.txt
step "Windows x64 cross build"
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release > $R/configure.txt 2>&1 || { cat $R/configure.txt; exit 1; }
cmake --build build-win > $R/build.txt 2>&1; RES[build]=$?; grep -c "warning:" $R/build.txt | sed 's/^/compiler warnings: /' ; [ "${RES[build]}" = 0 ] || { tail -30 $R/build.txt; exit 1; }
grep -E "warning:" $R/build.txt | grep -v "third_party" | head -5
step "core tests (Windows binary under Wine)"; (cd build-win && timeout 300 $WINE ./BodySyncTests.exe) > $R/wine-core.txt 2>&1; RES[wine_core]=$?; tail -2 $R/wine-core.txt
step "driver mock host + end-to-end (real driver DLL, real pipe)"; (cd build-win && timeout 300 $WINE ./DriverHarness.exe) > $R/driver.txt 2>&1; RES[driver]=$?; tail -2 $R/driver.txt
step "application self-test (headless UI, five languages)"; (cd build-win/stage && timeout 300 $WINE ./BodySync.exe --selftest --out "Z:/$ROOT/$R/selftest.txt") > $R/selftest-stdout.txt 2>&1; RES[selftest]=$?; tail -3 $R/selftest.txt 2>/dev/null
step "driver registration helper"; tests/test_setup_tool.sh > $R/setup-tool.txt 2>&1; RES[setup]=$?; tail -1 $R/setup-tool.txt
if [ $PACKAGE = 1 ]; then
  step "package"; scripts/package.sh > $R/package.txt 2>&1 || { cat $R/package.txt; exit 1; }; tail -8 $R/package.txt
  step "installer (real NSIS installer under Wine)"; tests/test_installer.sh > $R/installer.txt 2>&1; RES[installer]=$?; tail -1 $R/installer.txt
fi
bad=0; for k in "${!RES[@]}"; do [ "${RES[$k]}" = 0 ] || { echo "FAILED: $k"; bad=1; }; done
[ $bad = 0 ] && echo "ALL TESTS PASSED" || { echo "SOME TESTS FAILED"; exit 1; }
