#!/usr/bin/env bash
# Runs BodySync-Setup.exe silently under Wine against a fake SteamVR paths file, checks the result, then uninstalls.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; SETUP="${1:-$ROOT/dist/BodySync-Setup.exe}"
. "$ROOT/scripts/wine-env.sh"
# the NSIS installer stub is 32-bit: needs a WoW64 prefix and the 32-bit capable `wine` launcher (apt: wine32:i386)
export WINEPREFIX="${INSTALLER_WINEPREFIX:-$HOME/.wine-inst}"; WINE=$(command -v wine)
T=$(mktemp -d); fail=0; ok(){ echo "  ok: $1"; }; bad(){ echo "  FAIL: $1"; fail=1; }
LA="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Local"; mkdir -p "$LA"; [ -e "$LA/openvr" ] && mv "$LA/openvr" "$T/openvr.saved"
[ -e "$LA/BodySync" ] && mv "$LA/BodySync" "$T/BodySync.saved"
cleanup(){ rm -rf "$LA/openvr" "$LA/BodySync"; [ -e "$T/openvr.saved" ] && mv "$T/openvr.saved" "$LA/openvr"; [ -e "$T/BodySync.saved" ] && mv "$T/BodySync.saved" "$LA/BodySync"; rm -rf "$T"; }; trap cleanup EXIT
mkdir -p "$LA/openvr"; V="$LA/openvr/openvrpaths.vrpath"
echo '{ "external_drivers" : [ "C:\\\\other\\\\driver" ], "runtime" : [ "C:\\\\NoSuchSteamVR" ], "version" : 1 }' > "$V"
mkdir -p "$LA/BodySync/profiles"; echo '{"keep":"me"}' > "$LA/BodySync/profiles/keep.json"
DEST="$T/instdir"; WDEST="Z:${DEST//\//\\}"
timeout 180 $WINE "$SETUP" /S /D=$WDEST >/dev/null 2>&1; echo "  installer exit code: $?"
for f in BodySync.exe bodysync_setup.exe openvr_api.dll Uninstall-BodySync.exe driver/bodysync/driver.vrdrivermanifest driver/bodysync/bin/win64/driver_bodysync.dll \
         driver/bodysync/resources/settings/default.vrsettings fonts/NotoSans-Regular.ttf Install.bat Uninstall.bat LICENSE.txt LICENSES.md README.md docs/INSTALL.md LICENSES/OpenVR-LICENSE.txt; do
  [ -s "$DEST/$f" ] && ok "installed $f" || bad "missing $f"; done
grep -q "instdir" "$V" && ok "driver registered in the SteamVR paths file" || bad "driver not registered"
grep -q "other" "$V" && ok "foreign driver entry preserved" || bad "foreign entry lost"
$WINE reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\BodySync' /v DisplayName 2>/dev/null | grep -q BodySync && ok "uninstall entry in the registry" || bad "no uninstall registry entry"
# program starts and runs its own self-test from the installed location
timeout 120 $WINE "$DEST/BodySync.exe" --selftest --out "$T/st.txt" >/dev/null 2>&1; grep -q "RESULT: PASSED" "$T/st.txt" && ok "installed BodySync.exe passes --selftest" || bad "selftest from installed copy"
# uninstall: keeps user data
timeout 120 $WINE "$DEST/Uninstall-BodySync.exe" /S "_?=$WDEST" >/dev/null 2>&1
[ ! -e "$DEST/BodySync.exe" ] && [ ! -e "$DEST/driver" ] && [ ! -e "$DEST/fonts" ] && ok "program files removed" || bad "program files remain"
! grep -q "instdir" "$V" && ok "driver registration removed" || bad "driver registration remains"
grep -q "other" "$V" && ok "foreign driver entry still present" || bad "foreign entry lost on uninstall"
[ -s "$LA/BodySync/profiles/keep.json" ] && ok "user data kept by default" || bad "user data deleted without consent"
# reinstall + uninstall with /PURGE
timeout 180 $WINE "$SETUP" /S /D=$WDEST >/dev/null 2>&1; timeout 120 $WINE "$DEST/Uninstall-BodySync.exe" /S /PURGE "_?=$WDEST" >/dev/null 2>&1
[ ! -e "$LA/BodySync" ] && ok "/PURGE removes user data" || bad "/PURGE did not remove user data"
[ $fail = 0 ] && echo "installer tests PASSED" || { echo "installer tests FAILED"; exit 1; }
