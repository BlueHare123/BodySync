# BodySync

**EN** · [RU](#русский)

BodySync is a Windows x64 program that gives SteamVR eight virtual full-body trackers (waist, chest, both elbows, both knees, both feet) estimated from the headset and the two controllers. Physical trackers can be mixed in per role: when one is healthy it takes over, when it is lost BodySync blends back to the estimate.

It consists of three parts:

| Part | File | Job |
|---|---|---|
| Application | `BodySync.exe` | Reads the headset, controllers and trackers through the public OpenVR client API, solves the body pose, shows the user interface and sends the result to the driver. |
| SteamVR driver | `driver\bodysync\` (`driver_bodysync.dll`) | Registers eight `GenericTracker` devices in SteamVR and publishes the poses it receives from the application through a local named pipe. |
| Helper | `bodysync_setup.exe` | Registers / unregisters the driver folder with SteamVR (uses SteamVR's `vrpathreg`, or edits `openvrpaths.vrpath` after making a backup). |

## What it does

- Body solver for standing, sitting and lying (four variants), automatic posture detection with hysteresis and hold times, smooth transitions between postures. Lying is detected on beds and sofas (with a pillow); squats move the hips back and lean the torso; **sitting style** selectable: chair, upright, leaning back, cross-legged on the floor.
- Feet: planting, stepping, turning in place, walking in place, ground contact.
- Mixed tracking: assign physical trackers to roles, priorities (physical/virtual preferred or only), glitch / freeze / loss detection, debounced recovery, smooth hand-over.
- Calibration wizard (quick and advanced), manual adjustment of every proportion and offset, presets.
- Profiles, body / pose / calibration presets, import / export, autosave, safe handling of damaged files.
- Dark interface in **English, Russian, German, French and Spanish** (bundled Noto Sans font, adjustable interface scale); every setting visible in the interface changes behaviour (checked by automated tests).
- A clearly labelled **simulation** mode for checking the solver and the pipeline without a headset. Simulated poses never reach SteamVR unless you explicitly allow it on the Diagnostics page.

## Quick start

1. Install with `BodySync-Setup.exe` (no administrator rights needed), or unpack `BodySync-Windows-x64.zip` somewhere permanent and run `Install.bat`.
2. Start SteamVR once (so it creates its paths file), then start BodySync.
3. Open **Calibration**, run the quick calibration, check the result.
4. In SteamVR open *Manage Trackers* and make sure the eight BodySync trackers have the roles waist, chest, elbows, knees, feet (BodySync asks SteamVR for these roles, see INSTALL).

Details: [INSTALL](docs/INSTALL.md) · [CALIBRATION](docs/CALIBRATION.md) · [CONFIGURATION](docs/CONFIGURATION.md) · [TROUBLESHOOTING](docs/TROUBLESHOOTING.md) · [BUILD](BUILD.md) · [CHANGELOG](CHANGELOG.md) · [RELEASE NOTES](RELEASE_NOTES.md) · [LICENSES](LICENSES.md)

## Honest status

Verified by automated tests: the solver, mixer, calibration maths, configuration storage, the named-pipe protocol, the driver (loaded as a real DLL into a mock SteamVR host), the engine → pipe → driver chain, the setup helper, the installer (if it was built) and the user interface (rendered headless in all five languages and inspected under Wine).

**Not verified** (no SteamVR, VR headset, controllers, trackers or VRChat were available in the build environment): behaviour inside real SteamVR, with a real headset, with physical trackers, in VRChat or any other game, and on a native Windows machine. Read the known limitations in [RELEASE_NOTES](RELEASE_NOTES.md) before relying on it.

---

## Русский

BodySync — программа для Windows x64, которая добавляет в SteamVR восемь виртуальных трекеров тела (таз, грудь, оба локтя, оба колена, обе стопы), рассчитанных по шлему и двум контроллерам. Физические трекеры можно подмешивать по ролям: пока трекер исправен, он главный; при потере BodySync плавно возвращается к расчёту.

Состав: `BodySync.exe` (приложение), `driver\bodysync\` (драйвер SteamVR), `bodysync_setup.exe` (регистрация драйвера в SteamVR).

**Возможности:** расчёт позы стоя / сидя / лёжа (4 варианта) с автоматическим определением и плавными переходами; шаги, повороты, ходьба на месте; смешанное отслеживание с физическими трекерами; мастер калибровки (быстрая и расширенная) и ручная настройка; профили и пресеты, импорт и экспорт, автосохранение; тёмный интерфейс на английском, русском, немецком, французском и испанском; выбор стиля сидения (стул, прямо, откинувшись, на полу со скрещёнными ногами); определение положения лёжа на кровати/диване; режим **симуляции** для проверки без шлема (в SteamVR не отправляется, пока вы сами этого не разрешите).

**Быстрый старт:** 1) установите `BodySync-Setup.exe` (права администратора не нужны) или распакуйте ZIP в постоянную папку и запустите `Install.bat`; 2) один раз запустите SteamVR, затем BodySync; 3) на странице «Калибровка» выполните быструю калибровку; 4) в SteamVR → «Управление трекерами» проверьте роли восьми трекеров BodySync.

**Что не проверено:** работа в настоящем SteamVR, с реальным шлемом, контроллерами и физическими трекерами, в VRChat и на настоящей машине с Windows — в среде сборки их не было. Подробности и известные ограничения — в [RELEASE_NOTES](RELEASE_NOTES.md).
