#!/usr/bin/env bash
# Downloads pinned third-party sources into third_party/ and verifies SHA-256 (deps.sha256).
# Needs: curl, sha256sum. Run from anywhere. Use --update to regenerate the checksum file.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; TP="$ROOT/third_party"; R=https://raw.githubusercontent.com
OPENVR=v2.12.14; IMGUI=v1.91.9; JSON=v3.11.3
get(){ mkdir -p "$(dirname "$TP/$2")"; [ -s "$TP/$2" ] || curl -fsSL --retry 3 -o "$TP/$2" "$R/$1"; }
for f in openvr.h openvr_driver.h; do get ValveSoftware/openvr/$OPENVR/headers/$f openvr/headers/$f; done
get ValveSoftware/openvr/$OPENVR/bin/win64/openvr_api.dll openvr/bin/win64/openvr_api.dll
get ValveSoftware/openvr/$OPENVR/LICENSE openvr/LICENSE
for f in imgui.cpp imgui.h imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_internal.h imconfig.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h LICENSE.txt \
         backends/imgui_impl_win32.cpp backends/imgui_impl_win32.h backends/imgui_impl_dx11.cpp backends/imgui_impl_dx11.h; do get ocornut/imgui/$IMGUI/$f imgui/$f; done
get nlohmann/json/$JSON/single_include/nlohmann/json.hpp json/nlohmann/json.hpp
get nlohmann/json/$JSON/LICENSE.MIT json/LICENSE.MIT
cd "$TP"
if [ "${1:-}" = "--update" ]; then find . -type f | sort | xargs sha256sum > "$ROOT/deps.sha256"; echo "deps.sha256 updated"; else sha256sum -c "$ROOT/deps.sha256" --quiet && echo "third_party OK"; fi
