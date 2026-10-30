#!/usr/bin/env bash
# Tests bodysync_setup.exe (driver registration) under Wine against a fake %LOCALAPPDATA%\openvr\openvrpaths.vrpath.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; STAGE="${1:-$ROOT/build-win/stage}"
. "$ROOT/scripts/wine-env.sh"
T=$(mktemp -d); fail=0
ok(){ echo "  ok: $1"; }; bad(){ echo "  FAIL: $1"; fail=1; }
# Wine ignores a Linux-side LOCALAPPDATA, so the fake file lives in the (test) prefix profile and is removed afterwards.
LA="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Local"; mkdir -p "$LA"; [ -e "$LA/openvr" ] && mv "$LA/openvr" "$T/openvr.saved"
mkdir -p "$LA/openvr"; V="$LA/openvr/openvrpaths.vrpath"
cleanup(){ rm -rf "$LA/openvr"; [ -e "$T/openvr.saved" ] && mv "$T/openvr.saved" "$LA/openvr"; rm -rf "$T"; }; trap cleanup EXIT
cat > "$V" <<J
{ "config" : [ "C:\\\\Steam\\\\config" ], "external_drivers" : [ "C:\\\\other\\\\driver" ], "jsonid" : "vrpathreg", "log" : [ "C:\\\\Steam\\\\logs" ], "runtime" : [ "C:\\\\NoSuchSteamVR" ], "version" : 1 }
J
DRV="$T/dr iver/bodysync"; mkdir -p "$DRV/bin/win64"; echo '{}' > "$DRV/driver.vrdrivermanifest"; : > "$DRV/bin/win64/driver_bodysync.dll"
WDRV="Z:${DRV//\//\\}"
run(){ timeout 60 $WINE "$STAGE/bodysync_setup.exe" "$@" 2>&1; }
count(){ python3 -c "import json,sys;print(len([d for d in json.load(open(sys.argv[1]))['external_drivers'] if 'bodysync' in d.lower()]))" "$V"; }
run register "$WDRV" | grep -q "Driver registered" && ok "register" || bad "register"
[ "$(count)" = 1 ] && ok "entry added once" || bad "entry count $(count)"
run register "$WDRV" | grep -q "already registered" && ok "register is idempotent" || bad "idempotent"
[ "$(count)" = 1 ] && ok "no duplicate" || bad "duplicate"
python3 -c "import json,sys;d=json.load(open(sys.argv[1]));assert 'C:\\\\other\\\\driver' in d['external_drivers'] and d['version']==1" "$V" && ok "foreign entries and keys preserved" || bad "foreign data damaged"
[ -f "$V.bodysync-backup" ] && ok "backup of the original written" || bad "no backup"
run check "$WDRV" | grep -q "driver registered: yes" && ok "check reports registered" || bad "check"
run unregister "$WDRV" | grep -q "Driver unregistered" && ok "unregister" || bad "unregister"
[ "$(count)" = 0 ] && ok "entry removed" || bad "entry still there"
python3 -c "import json,sys;d=json.load(open(sys.argv[1]));assert 'C:\\\\other\\\\driver' in d['external_drivers']" "$V" && ok "foreign entry still present" || bad "foreign entry lost"
run register "$T/nothing" | grep -q "not a BodySync driver folder" && ok "invalid folder refused" || bad "invalid folder accepted"
rm "$V"; run register "$WDRV" | grep -q "SteamVR path file not found" && ok "missing SteamVR reported clearly" || bad "missing SteamVR message"
run unregister "$WDRV" | grep -q "nothing to unregister" && ok "unregister without SteamVR is harmless" || bad "unregister without SteamVR"
echo '{ broken' > "$V"; run register "$WDRV" | grep -q "cannot parse" && ok "corrupt path file is not overwritten" || bad "corrupt handling"
[ "$(cat "$V")" = "{ broken" ] && ok "corrupt file untouched" || bad "corrupt file modified"
[ $fail = 0 ] && echo "setup tool tests PASSED" || { echo "setup tool tests FAILED"; exit 1; }
