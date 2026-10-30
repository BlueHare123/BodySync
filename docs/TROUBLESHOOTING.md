# Troubleshooting / Решение проблем

First stop: **Diagnostics** page → *Save diagnostic report*, and the log (`%LOCALAPPDATA%\BodySync\logs\bodysync.log`). The SteamVR log (`vrserver.txt`) contains lines starting with `[BodySync]` from the driver.

| Symptom | Cause / what to do |
|---|---|
| "SteamVR is not running" on the Dashboard | Start SteamVR. BodySync connects as a background application and retries every 2 s; it never starts SteamVR. |
| "BodySync driver: not detected" while SteamVR runs | The driver is not registered or SteamVR was started before it was. Run `Install.bat` / *Register SteamVR driver*, then restart SteamVR. Check `bodysync_setup.exe check "<folder>\driver\bodysync"`. Look for `[BodySync] registered 8 trackers` in `vrserver.txt`. |
| Registration says "SteamVR path file not found" | Start SteamVR once after installing it, then repeat. |
| Registration says "cannot parse openvrpaths.vrpath" | The file is damaged; BodySync does not overwrite it. Repair or delete it (SteamVR recreates it) and register again. |
| The eight trackers do not appear in SteamVR | The driver may be disabled (`driver_bodysync/enable` in `steamvr.vrsettings`), or blocked by security software. Check `vrserver.txt`. |
| Trackers exist but have no role | Set roles in SteamVR → Manage Trackers. |
| Body is shifted / turned / floats or sinks | Repeat *Floor* and *Stand*; use *Body yaw offset*, *Offset* and *Floor level* on the Calibration page. The poses are published in the headset driver's raw tracking space; setups with several tracking systems may need the offsets. |
| Knees or feet in the wrong place | Adjust *Leg length scale*, *Foot spacing*, *Knee splay*. |
| Pelvis jitters | Raise *Smoothing*; lower *Pelvis sway*. |
| Feet slide when you stand still | Raise *Foot lock*. Steps feel late: lower it. |
| Posture flips between sitting and standing | Widen the gap between *Sit enter ratio* and *Sit exit ratio*, raise *Hold time*. |
| A physical tracker is ignored | *Mixed Tracking* → role state shows the reason (no pose, poor quality, jump, frozen). Make sure the serial is assigned and priority is not *Virtual only*. |
| "Not sending trackers" | The switch *Send trackers to SteamVR* on the Dashboard is off, or the input source is Simulation. |
| Russian (or accented) text shows as `?` | The bundled font was not found. BodySync uses `fonts\NotoSans-Regular.ttf` next to `BodySync.exe`; do not delete the `fonts` folder (Segoe UI is only a fallback). |
| Windows SmartScreen / antivirus warns | The programs are not code-signed. Verify the source of the file. |
| Settings did not save | Autosave off? Use *Save now*. A toast shows write errors (folder read-only, disk full). |

BodySync does not read the memory of other processes, inject code or use undocumented interfaces; if a security product reports otherwise, report it as a false positive.

---

## Русский

Начните со страницы «Диагностика» → «Сохранить диагностический отчёт» и журнала (`%LOCALAPPDATA%\BodySync\logs\bodysync.log`); строки драйвера с `[BodySync]` — в `vrserver.txt` SteamVR.

- **«SteamVR не запущен»** — запустите SteamVR; BodySync подключится сам (раз в 2 с), сам SteamVR он не запускает.
- **«Драйвер BodySync: не обнаружен»** — драйвер не зарегистрирован или SteamVR запущен раньше. Выполните `Install.bat`, перезапустите SteamVR; проверка: `bodysync_setup.exe check "<папка>\driver\bodysync"`.
- **«SteamVR path file not found»** — один раз запустите SteamVR и повторите.
- **Трекеры без роли** — назначьте роли в SteamVR → «Управление трекерами».
- **Тело смещено/повёрнуто/«парит»** — повторите «Пол» и «Рост», используйте «Поворот тела», «Смещения», «Уровень пола».
- **Дрожит таз** — увеличьте «Сглаживание», уменьшите «Покачивание таза». **Ноги скользят** — увеличьте «Фиксацию ступней».
- **Поза «скачет» между сидя и стоя** — увеличьте разницу порогов входа/выхода и время удержания.
- **Физический трекер игнорируется** — на странице «Смешанное отслеживание» в столбце состояния указана причина.
- **Вместо русских букв «?»** — не найден шрифт с кириллицей; не удаляйте папку `fonts`.
