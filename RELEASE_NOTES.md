# BodySync v1.0.0

BodySync gives SteamVR eight virtual full-body trackers (waist, chest, elbows, knees, feet) estimated from the headset and controllers; physical trackers can be mixed in per role.

## Highlights
- Lying down is detected reliably on beds and sofas, with a pillow, in all four lying kinds.
- Natural squats: hips move back, torso leans, knees respect their limit, feet stay on the floor.
- Choose how you sit: chair, upright, leaning back, or cross-legged on the floor.
- Interface in English, Russian, German, French and Spanish; interface scale; bundled Noto Sans font.
- Profiles from earlier builds load unchanged.

## Files
- `BodySync-Windows-x64.zip` - portable build (run `Install.bat` once to register the driver).
- `BodySync-Setup.exe` - installer (only if attached to the release).
- `BodySync-Source.zip` - source for a reproducible build (see `BUILD.md`).

## Known limitations
- Posture and joint positions are estimates from the head and hands, not measurements.
- Sitting styles shape the estimated body; the real legs are not seen. Unusual positions (for example bending over with a still head) can be misjudged; *Always lying / standing / sitting* in Poses forces a posture.
- The executables and installer are not code-signed, so Windows SmartScreen may warn on first run.
- Translations are machine-assisted; corrections are welcome as issues.
