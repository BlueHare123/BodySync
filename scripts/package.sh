#!/usr/bin/env bash
# Assembles dist/: portable release folder, BodySync-Windows-x64.zip, BodySync-Source.zip and (if makensis exists) BodySync-Setup.exe.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
BUILD="${BUILD:-build-win}"; STAGE="$BUILD/stage"
VER=$(grep -oP 'project\(BodySync VERSION \K[0-9.]+' CMakeLists.txt)
REL=dist/BodySync; rm -rf dist; mkdir -p "$REL/docs" "$REL/LICENSES"
need(){ [ -s "$1" ] || { echo "missing or empty: $1" >&2; exit 1; }; }
for f in BodySync.exe bodysync_setup.exe openvr_api.dll driver/bodysync/driver.vrdrivermanifest driver/bodysync/bin/win64/driver_bodysync.dll \
         driver/bodysync/resources/input/bodysync_tracker_profile.json driver/bodysync/resources/settings/default.vrsettings fonts/NotoSans-Regular.ttf fonts/NotoSans-Bold.ttf; do need "$STAGE/$f"; done
cp -r "$STAGE/BodySync.exe" "$STAGE/bodysync_setup.exe" "$STAGE/openvr_api.dll" "$STAGE/driver" "$STAGE/fonts" "$REL/"
cp resources/bodysync.ico "$REL/BodySync.ico"; cp LICENSE "$REL/LICENSE.txt"; cp LICENSES.md README.md CHANGELOG.md RELEASE_NOTES.md BUILD.md "$REL/"; cp docs/*.md "$REL/docs/"
cp installer/portable/Install.bat installer/portable/Uninstall.bat "$REL/"
cp third_party/openvr/LICENSE "$REL/LICENSES/OpenVR-LICENSE.txt"; cp third_party/imgui/LICENSE.txt "$REL/LICENSES/ImGui-LICENSE.txt"
cp third_party/json/LICENSE.MIT "$REL/LICENSES/nlohmann-json-LICENSE.txt"; cp resources/fonts/NotoSans-OFL.txt "$REL/LICENSES/NotoSans-OFL.txt"
sed -i 's/$/\r/' "$REL/Install.bat" "$REL/Uninstall.bat"   # batch files need CRLF
{ echo "BodySync $VER"; echo "Built: $(date -u +%Y-%m-%dT%H:%MZ) (cross-built with mingw-w64, tested under Wine; not tested on native Windows or in real SteamVR)"; echo; (cd "$REL" && find . -type f \( -name '*.exe' -o -name '*.dll' \) | sort | xargs sha256sum); } > "$REL/VERSION.txt"
# release sanity: no empty files, driver manifest points at an existing DLL layout
find "$REL" -type f -empty | grep . && { echo "empty file in release" >&2; exit 1; } || true
python3 - "$REL" "$VER" <<'PY'
import sys, os, zipfile
rel, ver = sys.argv[1], sys.argv[2]
def zipdir(src, out, prefix):
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, dirs, files in os.walk(src):
            dirs.sort()
            for f in sorted(files):
                p = os.path.join(root, f); z.write(p, os.path.join(prefix, os.path.relpath(p, src)))
zipdir(rel, 'dist/BodySync-Windows-x64.zip', 'BodySync')
skip_dirs = {'build-win', 'build-native', 'build-results', 'dist', '.git', '__pycache__'}
with zipfile.ZipFile('dist/BodySync-Source.zip', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk('.'):
        dirs[:] = sorted(d for d in dirs if d not in skip_dirs)
        for f in sorted(files):
            p = os.path.join(root, f)
            if f.endswith(('.corrupt',)) or '.bodysync-backup' in f: continue
            z.write(p, os.path.join('BodySync-Source', os.path.relpath(p, '.')))
PY
if command -v makensis >/dev/null; then makensis -V2 -DSTAGE="$ROOT/$REL" -DVERSION="$VER" -DOUT="$ROOT/dist/BodySync-Setup.exe" installer/installer.nsi; else echo "makensis not found: installer EXE not built" >&2; fi
( cd dist && sha256sum *.zip *.exe 2>/dev/null > SHA256SUMS.txt; cat SHA256SUMS.txt; ls -la )
