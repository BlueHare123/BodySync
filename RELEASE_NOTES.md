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
- Never tested in real SteamVR, with a real headset, controllers or physical trackers, in VRChat or on a native Windows machine; everything was tested with simulated motion, a mock SteamVR host and Wine.
- Posture and joint positions are estimates from head and hands; they are not measurements. Accuracy on real people is unknown.
- Sitting styles shape the estimated body; they cannot see your real legs. Lying is told apart from bending over / looking up from head tilt, head height and hand position, so unusual positions can be misjudged (use *Always lying / standing / sitting* in Poses, or tune the thresholds).
- The installer and executables are not code-signed (Windows SmartScreen may warn).
- Translations were produced without a native-speaker review; please report wrong or awkward wording as an issue.
