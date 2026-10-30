#!/usr/bin/env bash
# Fast native (Linux) build of the portable core + tests. Used during development; the shipped binaries are cross-built for Windows.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:-build-native}; mkdir -p "$OUT"
g++ -std=c++20 -O2 -g -Wall -Wextra -Icore/include -Ithird_party/json -Itests \
  core/src/config.cpp core/src/config_store.cpp core/src/solver.cpp core/src/mixer.cpp core/src/calibration.cpp core/src/sim.cpp core/src/ipc_protocol.cpp \
  tests/test_main.cpp tests/test_math.cpp tests/test_solver.cpp tests/test_config.cpp tests/test_mixed.cpp tests/test_settings_effect.cpp tests/test_human_model.cpp tests/test_v100.cpp \
  -o "$OUT/bodysync-tests"
"$OUT/bodysync-tests" "${@:2}"
