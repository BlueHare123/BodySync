# Building from source / Сборка

The Windows binaries are **cross-built on Linux** with mingw-w64, and the automated tests run under Wine. They can also be built natively on Windows with MSYS2 (mingw-w64) using the same CMake files (see `scripts/build.ps1`).

## Linux (cross build, the tested route)

Requirements (Debian/Ubuntu names): `g++-mingw-w64-x86-64-posix`, `cmake` ≥ 3.20, `ninja-build`, `wine64` (tests only; the installer test also needs the 32-bit `wine`/`wine32`), `nsis` (installer), `python3`, `curl`, `xvfb` + `imagemagick` (only to take UI screenshots); the bundled fonts are in `resources/fonts` (Noto Sans, SIL OFL).

```bash
scripts/fetch_deps.sh        # downloads pinned third_party/* and verifies SHA-256 (already vendored in the source ZIP)
scripts/build.sh             # configure, build, run all tests (logs in build-results/)
scripts/build.sh --package   # additionally creates dist/BodySync-Windows-x64.zip, BodySync-Source.zip and BodySync-Setup.exe
```

Manual steps: `cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-win`. The result is staged in `build-win/stage/`.

`cmake/native-tests.sh` builds only the portable core and its tests natively (fast development loop).

## Windows (MSYS2)

Install MSYS2, then in the *MinGW x64* shell: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja`, run `scripts/fetch_deps.sh` (needs `curl`) and `cmake -S . -B build -G Ninja && cmake --build build`. MSVC is not supported (the link flags and some code assume GCC).

## Layout

`core/` portable math, solver, mixer, calibration, configuration, logger, IPC protocol + Windows pipe transport · `driver/` SteamVR driver and its mock-host test · `app/` engine, OpenVR source, UI · `installer/` helper, NSIS script, portable `.bat` files · `tests/` automated tests · `resources/` icon, manifests, fonts · `third_party/` pinned dependencies.

## Pinned versions

OpenVR v2.12.14, Dear ImGui v1.91.9, nlohmann/json v3.11.3 (checksums in `deps.sha256`). Toolchain used: GCC 13 (mingw-w64, POSIX threads), CMake 3.28+, Ninja, Wine 9.0, NSIS 3.09.

## Localisation

Texts are keyed by their English wording (`tr("...")` / `N_("...")` in `app/*.cpp`). Translations live in `app/lang_ru.inc`, `lang_de.inc`, `lang_fr.inc`, `lang_es.inc`, one `{"English", "Translation"},` per line. `python3 scripts/check_i18n.py` verifies that every text exists in every language with matching placeholders; `BodySync.exe --selftest` renders every page in all five languages and fails on a missing translation.
