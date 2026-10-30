# Changelog

All notable changes to BodySync are listed here. The version numbering was reset: the first release of this line is **v1.0.0**.

## [1.0.0] - 2026-10-30

### Fixed
- **Lying down was often not detected.** Root causes found and fixed: the head-height gate (0.45 of eye height) rejected normal beds and sofas (a head on a pillow is 0.45-0.55 of eye height above the floor); the head-tilt gate rejected a head resting on a pillow (neck flexion of about 40 degrees); the lying kind (back / front / left / right) flipped on small head movements. Detection now combines independent features (head tilt, head height with a generous limit, hands not hanging far below the head, low-passed head axes, hysteresis and a hold time), and keeps the current kind unless the other one clearly wins. Standing and looking at the ceiling, bending over, crouching and looking up and reclined sitting are not reported as lying.
- **Squats looked unnatural.** The hips sank straight down with an upright torso, the knees were pushed past their limit and the feet were pressed into the floor. With the feet planted the hips now move back and the torso leans forward as the head drops (spine length and the knee limit are solved together); a shallow knee bend is unchanged; the feet stay planted.
- Role names in the Body Tracking page were translated twice (a harmless lookup miss in the self-test).

### Added
- **Seated pose selection** (Poses page): chair (default, the previous seated shape), upright torso, relaxed / leaning back, cross-legged on the floor. The choice changes the pelvis, torso, knees and feet of the solved skeleton, is stored in the profile and preset, and *Restore default sitting settings* resets it. Posture detection itself is unchanged: being seated is still detected from the data.
- **Five interface languages:** English (default), Russian, German, French, Spanish. Language is selectable in the sidebar and remembered. The installer also speaks all five.
- **Interface scale** (80 %-175 %) in the sidebar, remembered.
- Bundled font **Noto Sans** (SIL OFL 1.1) covering all five languages; headings use Noto Sans Bold. The look no longer depends on Windows fonts.
- `scripts/check_i18n.py`: fails the build if a text is missing in any language, has wrong placeholders or stale entries.

### Changed
- Default lying thresholds: height ratio 0.70, enter / exit head up-vector 0.70 / 0.85. Profiles that still carry the old defaults (0.45 / 0.5 / 0.7) are upgraded on load; values you changed yourself are kept.
- Version reset to 1.0.0 (application, executables, installer, archives).
- The test report is no longer a separate document: results are summarised in this changelog / `PROJECT_STATE.md`.

### Tests (offline)
- Core tests 57 (about 40 800 checks), driver / pipe harness 9, application self-test 44 checks (headless UI in five languages), localisation check (441 keys x 4 languages), all run natively and under Wine. New: lying on beds of 0-0.8 m with and without a pillow in all four lying kinds, head jitter while lying, false-positive set, squat geometry (knee limit, floor contact, torso lean, foot slide, knee speed), seat styles, profile migration.
- **Not tested:** real SteamVR, a real headset / controllers / physical trackers, VRChat, a native Windows machine. See `RELEASE_NOTES.md`.
