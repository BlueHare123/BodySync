# source me: environment for running the Windows binaries under Wine on Linux (testing only; end users run natively on Windows)
export WINEDEBUG=-all WINEPREFIX=${WINEPREFIX:-$HOME/.wine} LC_ALL=C.UTF-8 LANG=C.UTF-8
WINE=$(command -v wine64 || echo /usr/lib/wine/wine64)
