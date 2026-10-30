# Configuration / Настройки

All settings live in the **profile** (one JSON file per profile in `%LOCALAPPDATA%\BodySync\profiles\`). The application settings (`settings.json`) hold the language (en, ru, de, fr, es), the interface scale, the active profile, *Autosave* and the simulation-publish switch. Values are clamped into the ranges below when loaded or edited; unknown keys are ignored and missing keys keep their defaults, so files from older or newer versions load.

Files are written atomically (temporary file, then rename) inside an envelope `{format, kind, version, checksum, data}`. A hand-edited file still loads (a warning appears); a damaged file is renamed to `*.corrupt-<time>`, defaults are loaded, and nothing is deleted.

## Body Tracking

| Setting | Range (default) | Effect |
|---|---|---|
| Trackers (8 check boxes) | on | A disabled tracker disappears from SteamVR |
| Smoothing | 0–1 (0.45) | One-Euro filter strength on every output; higher = steadier, more delay |
| Pelvis sway | 0–1 (0.5) | 0 pelvis stays over the feet, 1 follows the neck |
| Torso turn dead zone | 5–90° (35) | Head turn allowed before the torso follows |
| Torso follow time | 0.2–20 s (3) | How slowly the torso relaxes to the head direction |
| Foot lock | 0–1 (0.6) | How far a planted foot stays before a step is taken |
| Step height / duration scale | 0–0.4 m (0.12) / 0.4–2.5 (1) | Shape of steps |
| Walk-in-place sensitivity | 0–1 (0.5) | Head-bob detection threshold |
| Posture sensitivity | 0–1 (0.5) | How quickly postures switch |
| Knee / elbow max bend | 60–170° (145) | Joint limit |
| Spine max lean, neck twist, spine twist | 10–85° (60), 20–120° (75), 5–90° (40) | Limits |
| Max speed | 2–50 m/s (12) | Glitch limiter for inputs and outputs |
| Update rate | 30–240 Hz (90) | Solver / publish rate |
| Pose time offset | −100…100 ms (0) | `poseTimeOffset` sent to SteamVR |

## How the body is estimated (v1.0.0)

- **Elbows:** the elbow lies on the circle that keeps both arm segments exact; the swivel angle is chosen to hang low, flare slightly outwards, stay out of the torso and stay close to the previous frame (no flips). If a hand is out of reach the shoulder follows it a little.
- **Torso heading:** from the head (dead zone / follow time above), plus two cues: both hands held out in front say where the body faces, and while walking forwards the torso faces the direction of travel.
- **Squats:** with the feet planted the hips cannot come closer to the ankles than the knee limit allows, so as the head drops the hips move back and the torso leans forward (spine length stays exact); a shallow bend does not move the hips.
- **Lying:** told from looking up / bending over by head tilt, head height and hand position, with low-passed head axes, hysteresis and a hold time.
- **Feet:** planted until the body moves away; feet never cross the body midline and stay on the floor when a leg would be too short. The pelvis cannot be higher than the legs allow unless you really jump.

## Poses

Mode (Automatic / Always standing / sitting / lying); **sitting style** (chair, upright, relaxed / leaning back, cross-legged on the floor: shapes pelvis, torso, knees and feet of the seated body; stored in the profile and in pose presets; *Restore default sitting settings* resets it); sit enter/exit ratio (0.80 / 0.88 of standing eye height, exit is always ≥ enter + 0.02), crouch ratio, lying height ratio (0.70: the head is below this fraction of standing eye height; beds and sofas count), lying enter/exit (head up-vector, 0.70 / 0.85: the head must be tilted this far from vertical; the hands must not hang far below the head, otherwise it is bending over), hold time (0.6 s), transition time (0.7 s), standing/sitting lean, seat height/back offset, seated leg extension, seated feet spread, knee splay, knee bend lying on back/side, pelvis height when lying. Pose presets store this group.

## Mixed Tracking

Use physical trackers; blend time (0.4 s), loss timeout (0.25 s), jump limit (8 m/s), frozen-after (1 s); per role: tracker serial (one tracker can serve one role) and priority *Physical preferred / Virtual preferred / Physical only / Virtual only*. A tracker counts as *healthy* when it is tracked with good quality, does not jump and does not freeze; it must be healthy for the blend time before it takes over, and bad for the loss timeout before it is dropped.

## Calibration and proportions

See [CALIBRATION](CALIBRATION.md).

## Command line

`BodySync.exe [--selftest [--out file]] [--simulate] [--page 0-5] [--lang en|ru|de|fr|es] [--data-dir path] [--version] [--help]`

`--selftest` runs the built-in checks in a temporary data folder and returns 0 only if all passed.

## Diagnostics

*Diagnostics* shows connections, solver values and the log; *Save diagnostic report* writes a text file to `reports\` (contains tracker serial numbers and your profile — review before sharing). Logs rotate at 1 MiB × 3 files in `logs\`.

---

## Русский

**Как оценивается тело (v1.0.0).** Локоть выбирается на окружности допустимых положений: низко (гравитация), слегка наружу, не внутри торса и близко к предыдущему кадру (без «переворотов»). Направление торса берётся от головы, а также от рук (обе вытянуты вперёд) и от направления движения при ходьбе вперёд. Ступни не пересекаются и не «зависают» над полом.

Все настройки хранятся в **профиле** (`%LOCALAPPDATA%\BodySync\profiles\*.json`); язык, активный профиль, автосохранение и разрешение отправки симуляции — в `settings.json`. Значения ограничиваются допустимыми диапазонами (таблицы выше), неизвестные ключи игнорируются, отсутствующие берутся по умолчанию. Запись атомарная; повреждённый файл переименовывается в `*.corrupt-<время>`, загружаются значения по умолчанию, ничего не удаляется.

Страницы: «Отслеживание тела» (сглаживание, шаги, пределы, частота, смещение времени), «Позы» (режим, пороги, формы), «Смешанное отслеживание» (назначение физических трекеров, приоритеты, таймауты), «Калибровка». Параметры командной строки — см. выше; `--selftest` запускает встроенные проверки.
