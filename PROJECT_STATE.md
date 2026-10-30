# PROJECT_STATE - BodySync v1.0.0

Working notes for whoever continues the project. Facts only; everything below was produced in the build environment described at the end.

## 1. Initial state (audit of the supplied source, previously v1.0.1)
- C++20, CMake. `core/` (portable: math, solver, mixer, calibration, config, sim, IPC protocol + Windows pipe), `app/` (Win32 + D3D11 + Dear ImGui, OpenVR client source, engine thread), `driver/` (SteamVR driver DLL, 8 GenericTracker devices fed through a named pipe), `installer/` (NSIS script, `bodysync_setup.exe`, portable .bat), `tests/` (own minimal test framework), `third_party/` (OpenVR, ImGui, nlohmann/json, SHA-256 pinned).
- Pose estimation: `core/src/solver.cpp` (posture state machine, standing / sitting / lying generators, two-bone IK, foot stepping, filters). Localisation: English text as key, Russian map in `app/lang_ru.inc` (two languages only). Font: Segoe UI -> bundled DejaVu.
- Baseline before any change: native core tests 43 / 15 189 checks, Wine core tests 48 / 15 220, driver harness 9 / 123, self-test 39 checks, all passing. Toolchain (mingw-w64, Wine, NSIS) had to be installed in this environment; all worked.
- Known defects found (reproduced with `probe` programs on realistic data):
  1. Lying: with a head 0.65 m above the floor (ordinary bed + pillow) the posture stayed Standing for all four kinds; with 40 degrees of neck flexion on the back / front it stayed Standing at any height. Causes: `lieHeightRatio` 0.45 and `lieEnterUp` 0.5 too strict; kind chosen from raw per-frame axes.
  2. Squat: torso stayed vertical (0-4 degrees lean) in a deep squat, the pelvis sank to 0.23 m, the knee reached 149 degrees (limit 145) and the IK pushed the feet into the floor (y 0.020 vs ankle 0.068).
  3. Only one seated shape existed (no choice).
  4. UI texts in two languages; role names were translated twice (harmless lookup miss).
- Existing tests used synthetic lying with the head at 0.16 m and exactly horizontal, which is why the defects were invisible.

## 2. Changes (v1.0.0)
See `CHANGELOG.md`. Code: `solver.cpp` (`classify`, squat constraint in `genStanding`, `genSitting` styles), `config.h/.cpp/config_json.h` (`SitStyle`, new lying defaults, `migratePoseDefaults`, `uiScale`, language codes), `config_store` (migration on load), `app/` (sit style card, language list, scale, fonts, i18n for 5 languages), `installer.nsi` (5 languages), `scripts/check_i18n.py`, `tests/test_v100.cpp`.
- Version: `CMakeLists.txt` `project(BodySync VERSION 1.0.0)` drives exe metadata, UI, installer, archive names. No Git history exists in the supplied archive, so tags / history were not touched; create the new release tag (e.g. `v1.0.0`, or `v1.0.0-reset` if the old repository already has a `v1.0.0` tag - do not delete or move old tags).
- Settings format: no format version bump. New keys (`poses.sitStyle`, `uiScale`) default when missing; profiles with the OLD default lying thresholds (0.45 / 0.5 / 0.7) are upgraded, other values are kept.

## 3. Verification actually performed (this environment: Linux, GCC 13 / mingw-w64, Wine 9.0)
| Check | Result |
|---|---|
| Native core tests (`cmake/native-tests.sh`) | 52 tests, 40 830 checks, 0 failed (final run) |
| Core tests, Windows binary under Wine | 57 tests, 40 861 checks, 0 failed |
| Driver mock host + end-to-end | 9 tests, 123 checks, 0 failed |
| Application self-test (headless UI, en/ru/de/fr/es, both input sources, modals, wizard, report) | 44 checks, 0 failed |
| Driver registration helper tests | passed |
| Localisation check (`scripts/check_i18n.py`) | 441 keys, ru/de/fr/es complete, 0 problems |
| Installer (real NSIS installer under Wine: silent install, files, registration, selftest from installed copy, uninstall, /PURGE) | passed |
| Screenshots under Wine (Xvfb): Poses (de), Dashboard (fr), Body Tracking (fr), Mixed Tracking (de) inspected: no clipped or overlapping text, Noto Sans renders accents and Cyrillic. Spanish pages were not captured (the Wine/Xvfb start was flaky); Spanish is covered by the self-test and the Latin-1 font ranges. |  |
- New behaviour checks: lying detected for 4 kinds x 5 bed heights (0-0.8 m) x 3 neck flexions, stable under +-12 degrees head wobble, ends when standing; not triggered by looking up / bending over / crouch-looking-up / reclined sitting; squat: knee bend below the comfort limit, feet never below ankle height, torso lean > 12 degrees, feet slide < 5 cm, knee speed < 2.2 m/s, hips do not touch the floor; shallow bend keeps the torso upright; four seat styles change the body, stay finite with exact segment lengths, are detected as sitting; style persists in JSON and falls back safely.

## 4. NOT verified
Real SteamVR, a real headset / controllers / physical trackers, VRChat, a native Windows machine, behaviour on real people (all tests use synthetic motion). Thresholds (lying 0.70 / 0.70 / 0.85, hand-drop 0.22 x eye height, squat comfort knee angle 92 % of the limit) are reasoned from body geometry, not tuned on recorded data. Translations (de, fr, es) were machine-produced and not reviewed by native speakers. The installer and executables are unsigned. The release was not uploaded anywhere (no GitHub access was used).

## 5. Open points / ideas
- Walking: the existing stepping, foot lock and smoothing were audited by tests only (all pass); no change was made beyond the defects above.
- Bending over at the waist with a still head is classified as Sitting (pre-existing, unchanged).
- Spanish screenshot, native-speaker review of de/fr/es, recorded real-world lying / squat data to tune thresholds.
- DPI change at run time still keeps the start-up DPI (the interface-scale selector is the workaround).
