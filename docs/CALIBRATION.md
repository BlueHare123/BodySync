# Calibration / Калибровка

BodySync needs three things from you: where the floor is, how tall you are, and (optionally) how long your arms are. The wizard measures them from the headset and controllers; everything it measures can also be typed in by hand.

Calibration needs real devices: the wizard is disabled in simulation mode and while SteamVR is not running.

## Quick calibration (about 30 s)

1. **Floor.** Lay one controller flat on the floor and do not touch it. Press *Start* (3 s countdown, 2 s capture). The controller must be steady (movement above 5 cm is rejected). BodySync assumes the controller origin is 3 cm above the floor; if your floor looks too high or low, correct *Floor level* by hand.
2. **Stand.** Stand upright, feet together, look straight ahead, arms down. 3 s capture. The result is your eye height, your body height (eye height ÷ 0.93) and the head tilt of your headset.
3. **Verify.** Stand naturally and move a little for 4 s. BodySync checks that segment lengths stay constant, left and right are symmetric and the pelvis is calm. *Save calibration* writes the values to the active profile. If the check finds problems you can repeat it or *Save anyway*.

## Advanced calibration

Adds a **T-pose** step between *Stand* and *Verify*: hold both arms straight out to the sides at shoulder height, look straight ahead (3 s). It measures arm length (scale 0.8 – 1.25 of the default) and the rotation of your torso against your head direction.

## Manual adjustment (Calibration page)

| Setting | Effect |
|---|---|
| Height, leg / torso / arm / width scale | body proportions; every limb length derives from them |
| Measured eye height | 0 = derived from height |
| Floor level | height of the floor in tracking space |
| Headset tilt | compensates a headset that points up/down when you look straight |
| Torso yaw bias | rotation of torso against head direction |
| Body yaw offset, offset right/up/forward | moves/rotates the whole virtual body (use if the trackers appear shifted or turned in the game) |
| Pelvis offset | position of the waist tracker inside the pelvis |
| Foot spacing / forward offset / toe-out | stance of the feet |

*Reset calibration* returns the calibration values (not the body proportions) to their defaults after a confirmation. Body presets and calibration presets save and load these groups under a name; presets are never overwritten unless you choose *Overwrite*.

## Tips

- Re-run *Floor* after moving your play area or re-setting up SteamVR.
- If the knees or feet look wrong in a mirror, adjust *Leg length scale* first.
- Calibrate with the headset worn the way you play.

---

## Русский

**Быстрая калибровка (~30 с):** 1) *Пол* — положите один контроллер ровно на пол и не трогайте (3 с отсчёт, 2 с запись); BodySync считает, что начало координат контроллера на 3 см выше пола; 2) *Рост* — встаньте прямо, ноги вместе, смотрите вперёд, руки опущены (3 с); из высоты глаз рассчитывается рост (÷ 0,93) и наклон шлема; 3) *Проверка* — 4 с стойте естественно и немного двигайтесь; проверяются постоянство длин сегментов, симметрия и устойчивость таза. «Сохранить калибровку» записывает значения в активный профиль.

**Расширенная:** добавляет Т-позу (руки в стороны на уровне плеч, 3 с) для длины рук и поворота торса.

**Вручную:** все значения (рост, масштабы, высота глаз, пол, наклон шлема, поворот и смещения тела, смещение таза, расстояние/разворот ступней) можно задать на странице «Калибровка»; есть пресеты тела и калибровки. Мастер недоступен в режиме симуляции и пока SteamVR не запущен.
