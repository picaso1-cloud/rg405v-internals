# Power & service audit + telemetry watchdog (2026-11-12)

**Device:** RG405V · GammaOS Next v1.1 Lite (Android 14) · USB ADB
**Mode:** read-only audit + two safe changes + a resident watchdog.
**Verdict:** the device is **already lean**. No rogue service, no wakelock
leak, idle drain ≈ **0–8 mA**. Do not strip more system services.

---

## 1. Audit results

### Battery / idle (from `dumpsys batterystats --charged`)

| Metric | Value | Reading |
|---|---|---|
| Uptime covered | 6 h 34 min | |
| Computed drain | **60 mAh** | |
| Actual drain | **0–50 mAh** | ≈ 0–7.7 mA average |
| Display | 0.0002 mAh | screen off most of the time |
| CPU | 0.018 mAh | negligible |
| **Sensors** | **1.02 mAh** | UID 1000, 5 h 59 min — **significant-motion for Doze**, expected |
| Wi-Fi | 0.0048 mAh | negligible |
| Wake locks | 0.075 mAh (UID 0, 41 min) | root/daemon; no user wakelock |
| GPU | 0.003 mAh | idle |
| `dumpsys power` Wake Locks | **size=0** | no leak |

### Doze

- Light + deep idle enabled and working; history shows repeated
  `deep-idle` / `deep-maint` cycles.
- `wait_for_unlock=true`, motion sensor = `Significant Motion` (Sprd).
- System whitelist includes `com.android.messaging`, `org.lineageos.etar`,
  `com.stevesoltys.seedvault`, DeskClock, Emergency, providers, Shell.
- `com.android.messaging` was removed from the **`system`** whitelist via
  `cmd deviceidle sys-whitelist -com.android.messaging`; a
  **`system-excidle`** entry remains (config-level, not runtime-removable).
  No telephony exists, so it is harmless either way.

### Running services (all core/foreground)

`systemui` (6), `Daijishō` (4), `settings` (2), `com.android.se` (2),
NetworkStack + tethering, Quickstep, media/FUSE, location.fused, Launcher3,
Latin IME, Bluetooth. **No GApps, no telephony, no NFC daemon.**

Already disabled before this audit: `com.android.egg`,
`com.android.bookmarkprovider`, `com.android.dreams.basic`,
`com.android.printspooler`, `com.android.nfc`,
`com.android.wallpaper.livepicker`.

### CPU at sample

`system_server` (dumpsys work), `adbd --root_seclabel` (this session),
`kworker/4:1+events_freezable`, `android.system.suspend-service`, `spi4`
(sensor hub), `Daijishō` (foreground), `logd`, `sugov:0`. No rogue process.
`spi4` used ~15 min CPU over ~9 h; the sensors line accounts for only
**0.16 mA**, so it is not worth touching.

### Other settings

| Setting | Value | Note |
|---|---|---|
| Screen timeout | **240 s** → **120 s** | changed (see §2) |
| Brightness | 102/255 | already low |
| Location mode | 3 (high accuracy) | optional: 2 or 0 if unused |
| Wi-Fi scan always | 0 | good |
| BLE scan always | null/unset | fine |
| `adaptive_battery_management_enabled` | null | GSI default |
| `cached_apps_freezer` | null (prop absent) | GSI default |
| Magisk modules | none | nothing to conflict |
| Daijishō dexopt | `speed-profile` | already optimal |
| Launcher3 dexopt | `speed` | already optimal |

---

## 2. Changes applied

1. **Screen timeout 240 s → 120 s**
   ```sh
   settings put system screen_off_timeout 120000
   # revert: settings put system screen_off_timeout 240000
   ```
2. **Removed `com.android.messaging` from the system Doze whitelist**
   ```sh
   cmd deviceidle sys-whitelist -com.android.messaging
   # revert: cmd deviceidle sys-whitelist +com.android.messaging
   ```
   (`system-excidle` remains; no telephony → no impact.)

**Not changed:** Bluetooth (controllers), location (apps may use it),
`com.android.se`, tethering, NetworkStack, LMK/zram (already tuned),
sensors/Doze motion (needed for idle), fan default (off = better battery;
enable `cool`/`max` only for gaming).

---

## 3. Telemetry watchdog

A root daemon samples every **10 min** and writes to
`/data/adb/rg405v-watch/` (survives reboot; root-only).

| File | Content |
|---|---|
| `watch.log` | timestamped snapshots, rotates at 5 MiB → `.1` … `.4` |
| `watch.log.1…4` | previous logs |
| `batterystats-YYYYMMDD-HHMM.csv` | `dumpsys batterystats -c` every 6 h, keeps 8 |
| `watch.pid` | daemon PID |

### Install / control

```sh
adb push tools/rg405v-watch.sh /data/adb/rg405v-watch.sh
adb push tools/service.d/61-rg405v-watch.sh /data/adb/service.d/61-rg405v-watch.sh
adb shell chmod 755 /data/adb/rg405v-watch.sh /data/adb/service.d/61-rg405v-watch.sh

adb shell /data/adb/rg405v-watch.sh once     # one sample
adb shell /data/adb/rg405v-watch.sh start    # daemon (service.d does this at boot)
adb shell /data/adb/rg405v-watch.sh status
adb shell /data/adb/rg405v-watch.sh tail
adb shell /data/adb/rg405v-watch.sh stop
```

Verified 2026-11-12: `start` → `running pid=13064 interval=600s`; a sample is
written immediately, then every 10 min. The `service.d` entry waits 30 s after
boot, then calls `start` (idempotent).

### What each sample contains

Battery (level/status/voltage/temp/charge counter/current), power state +
wakelocks, Doze state (`mState`, `mLightState`, screen/charging/network),
meminfo + zram `mm_stat`, PSI (cpu/memory/io), `top` (PID/CPU/RSS), thermal
status + `soc/cputop/gpu/board` zones, CPU governors/frequencies (policy0/6),
GPU frequency, fan mode/PWM/enable, foreground activity, pending-alarm counts
per UID.

### Pull / archive

```sh
adb root && adb pull /data/adb/rg405v-watch ./rg405v-watch-$(date +%F)
```

Keep one archive per week; the log is small (~1–2 MB/day at 10-min samples).

---

## 4. How to read the logs (optimization logic)

Compare **24 h windows**, same usage profile (idle vs gaming), one change at a
time. Look for:

| Signal | Healthy | Investigate |
|---|---|---|
| Idle drain (`batterystats` actual) | ≤ 10 mA | **> 15 mA** |
| Wake locks | 0–1 min/h | **> 5 min/h** with no app using it |
| Pending alarms per UID | few per hour | **> 100/h** or one UID growing |
| PSI `memory full` | ~0 | **> 1 %** sustained |
| PSI `io some` | low | spikes during ROM reads |
| Thermal `soc` | < 60 °C idle | **> 80 °C** under load |
| CPU idle | > 80 % | < 60 % with screen off |
| zram used | < 1 GB | **> 1.5 GB** (thrash risk) |
| Fan | off/cool; off at idle | max at idle = noise/battery |

Correlate `Pending alarms per uid` with the per-UID mAh in the 6-hourly
`batterystats-*.csv`; a UID with both high alarms and high mAh is the culprit.
`dumpsys batterystats -c` is the machine-readable checkin format; parse the
`9,<uid>,...` rows or use Battery Historian.

### A/B protocol

1. Baseline 24 h (no change).
2. Change **one** thing; reboot if it is a setting that applies at boot.
3. 24 h with the same usage; compare mAh/day, wakelocks, alarms, PSI, temps.
4. Keep only if the delta is > 5 % and nothing regressed.

---

## 5. Future candidates (not applied)

- **Location off / battery saving** if nothing uses it (mode 3 → 2 or 0).
- Remove `org.lineageos.etar` / `com.stevesoltys.seedvault` from the system
  whitelist **only if** calendar/Seedvault backups are unused (config-level
  `system-excidle` may need a ROM overlay, not a runtime command).
- Disable `com.android.se` if no eSIM/SE use (risky; leave unless measured).
- App-standby buckets for Aurora/Firefox (restrict background) — only if the
  logs show they wake often.
- Fan curve: `cool` for long sessions; it does not affect idle.
- Screen brightness 102 is already low; no change.

Do **not** add “battery saver” Magisk modules or disable Doze motion sensors:
the audit shows they would trade function for noise.

---

## 6. Revert everything

```sh
settings put system screen_off_timeout 240000
cmd deviceidle sys-whitelist +com.android.messaging
/data/adb/rg405v-watch.sh stop
rm -f /data/adb/service.d/61-rg405v-watch.sh
# keep /data/adb/rg405v-watch/ for the history, or rm -rf it
```

---

## 7. First real session — 2026-11-12

Window: **13:39–14:29 UTC**. PSP (PPSSPP) at 13:39, **ARMSX2 nightly
13:46:16–14:20:55**, stable ARMSX2 14:21–14:54, ARMSX3 from 14:54.

| Metric | Value | Reading |
|---|---|---|
| Peak SoC | **68.4 °C** (13:59) | cputop 63.4, GPU 63.0, board 51.3; `Thermal Status: 0` |
| Peak battery current | **658–702 mA** | 13:59, GTA SA; 312 mA at 14:09 |
| Average over 78 min | **~385 mA** (100→90 %) | ~10 % / 1.3 h |
| Fan | **off** (`auto`, pwm 204, `en=0`, `power=0`) | never enabled; trip is 70 °C |
| Memory | MemAvailable min **1.32 GB**; zram **595 MB** | PSI memory 0.00 — no pressure |
| Wakelocks | 2–3 | display + AudioMix/AudioOffload, expected |
| Renderer | **OpenGL** (`GL_RENDERER: Mali-G52`) | GTA SA had no per-game Vulkan override |

**Findings**

1. **The fan never ran.** `auto` keeps `pwm_en=0`/`fan_power=0` until the 70 °C
   trip; the session peaked just below it. For PS2 use `cool` (pwm 204) or
   `max` (255) from the start and verify `pwm_en=1`, `fan_power=1`.
2. **This was not a MaliSX2 run.** GTA SA (`SLUS-20946`) inherited global
   `Auto` → OpenGL; only CvS2 (`SLUS-20246`) has a per-game `vulkan` override.
   Emulog proves `renderer=OpenGL`, so the 68 °C/650 mA numbers are the
   **OpenGL baseline**, not the custom Vulkan driver.
3. **No memory pressure.** zram grew to 595 MB, MemAvailable stayed above
   1.3 GB, PSI memory stayed 0.00. The LMK never had to act.
4. **Watchdog gap.** No samples from **10:49 to 13:18** (148 min) while uptime
   advanced normally: a `dumpsys`/`top` call blocked the loop. Add
   `timeout 15` around each command and a heartbeat line. The gaming window
   itself was sampled every 10 min.

**Actions**

```sh
# fan for PS2 sessions
setprop persist.gammaos.fan_mode cool      # or max
cat /sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm_en
cat /sys/devices/platform/singleadc-joypad/fan_power

# renderer: nightly -> Graphics -> Renderer -> Vulkan (global or per game)
# verify in emulog.txt: "VK: driver is malisx2" / "renderer=Vulkan"
```
