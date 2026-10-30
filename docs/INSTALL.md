# Installation / Установка

## Requirements / Требования

- Windows 10 or 11, 64-bit. Direct3D 11 (a software fallback is used if no GPU driver is available).
- SteamVR installed through Steam and **started at least once** (it creates `%LOCALAPPDATA%\openvr\openvrpaths.vrpath`, which the driver registration needs).
- No administrator rights are needed. BodySync never closes SteamVR.

## Option A: installer / Установщик

1. Run `BodySync-Setup.exe`. Pick English, Russian, German, French or Spanish; keep **Register the BodySync driver in SteamVR** ticked.
2. Default folder: `%LOCALAPPDATA%\Programs\BodySync`. Start-menu entries: BodySync, *Register SteamVR driver*, *Uninstall BodySync*.
3. Silent install: `BodySync-Setup.exe /S` (optionally `/D=C:\target`, `/D=` must be the last argument).
4. The installer is **not code-signed**, so Windows SmartScreen may show a warning: choose *More info → Run anyway* only if you obtained the file from a source you trust.

Uninstall: *Settings → Apps*, or `Uninstall-BodySync.exe`. It removes the driver registration and the program files. Your settings, profiles and reports in `%LOCALAPPDATA%\BodySync` are kept unless you answer *Yes* to the question (silent uninstall: add `/PURGE` to delete them).

## Option B: portable ZIP / Портативный ZIP

1. Unpack `BodySync-Windows-x64.zip` to a **permanent** folder (SteamVR loads the driver from where it is registered). Do not unpack into a temporary or synchronised folder.
2. Run `Install.bat`. It runs `bodysync_setup.exe register "<folder>\driver\bodysync"`.
3. Start `BodySync.exe`.

Uninstall: run `Uninstall.bat` (removes the registration, asks before touching your data), then delete the folder.

## What the registration does

`bodysync_setup.exe register <driver folder>`:

1. Checks that the folder contains `driver.vrdrivermanifest` and `bin\win64\driver_bodysync.dll`.
2. Reads `%LOCALAPPDATA%\openvr\openvrpaths.vrpath` (`runtime` tells where SteamVR lives).
3. Uses SteamVR's own `vrpathreg.exe adddriver` when it exists; otherwise adds the folder to `external_drivers` in that file after saving a backup (`openvrpaths.vrpath.bodysync-backup`). Other entries are preserved; a damaged file is never overwritten.
4. If SteamVR is running it tells you to restart it; it does not close SteamVR.

`check [folder]` prints the state, `unregister <folder>` reverses step 3.

## Tracker roles

The driver registers eight trackers named `BodySync_Waist` … `BodySync_RightFoot` and asks SteamVR to give them the roles *Waist, Chest, Left/Right Elbow, Left/Right Knee, Left/Right Foot* (settings section `trackers`, only for roles you have not set yourself). If the roles do not appear automatically, set them in *SteamVR → Settings → Controllers → Manage Trackers*. This automatic assignment could not be verified in real SteamVR.

## Where data lives

`%LOCALAPPDATA%\BodySync\` — `settings.json`, `profiles\`, `body\`, `poses\`, `calibration\`, `reports\`, `logs\`. The `BODYSYNC_DATA_DIR` environment variable or `--data-dir <path>` redirects it.

---

## Русский

**Требования:** Windows 10/11 x64; SteamVR из Steam, запущенный **хотя бы один раз** (он создаёт `%LOCALAPPDATA%\openvr\openvrpaths.vrpath`). Права администратора не нужны; BodySync никогда не закрывает SteamVR.

**Установщик:** запустите `BodySync-Setup.exe`, выберите язык, оставьте галочку регистрации драйвера. Тихая установка: `/S`. Установщик не подписан цифровой подписью — SmartScreen может показать предупреждение. Удаление — через «Параметры → Приложения» или `Uninstall-BodySync.exe`; настройки и профили в `%LOCALAPPDATA%\BodySync` сохраняются, если не ответить «Да» на вопрос (при тихом удалении — ключ `/PURGE`).

**Портативный ZIP:** распакуйте в постоянную папку, запустите `Install.bat`, затем `BodySync.exe`. Удаление: `Uninstall.bat`, затем удалите папку.

**Роли трекеров:** драйвер просит SteamVR назначить роли (таз, грудь, локти, колени, стопы). Если роли не появились, назначьте их в SteamVR → «Управление трекерами». Автоназначение в настоящем SteamVR проверить не удалось.
