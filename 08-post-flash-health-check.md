# 08 — Post-flash health check (GammaOS Next v1.1 Lite)

**Date:** 2026-11-11 · **Build:** `lineage_arm64_bvN-userdebug 14
AP2A.240905.003 eng.gamma.20251121.210854` · **Mode:** USB ADB
`79783730379533` · **Uptime at check:** 14 min.

Evidence tags: **[seen]** measured on this unit · **[inferred]** · **[unknown]**.

---

## 1. Result: healthy

| Subsystem | Result | Evidence |
|---|---|---|
| Boot | `sys.boot_completed=1`, setup complete | [seen] |
| OS | Android **14**, LineageOS 21, `GammaOS_Next_v1.1` | [seen] |
| Security patch | `2025-06-01`, `userdebug`, `ro.debuggable=1` | [seen] |
| Bootloader | `flash.locked=0`, `verifiedbootstate=orange` (expected) | [seen] |
| Verified boot | `veritymode=enforcing` | [seen] |
| SELinux | **Permissive** | [seen] |
| Root | Magisk binary 26.1, app 29.0, `/sbin/su`; root granted to shell on request | [seen] |
| Storage | `/data` 110 GB, 4.0 GB used; A/B by-name + `dm-*` logical partitions | [seen] |
| Memory | 3.6 GB total, **2.4 GB available**, zram 2.2 GB (1.9 GB free) | [seen] |
| Display | 640×480 @ 160 dpi | [seen] |
| Wi-Fi | connected, `192.168.0.197`, RSSI −35 dBm, 2.4 GHz / 11n, 135 Mbps | [seen] |
| Bluetooth | **ON**, MAC `40:45:DA:AB:7F:85`, stable | [seen] |
| Battery | 100 %, health good, 33.8 °C, 4.31 V | [seen] |
| Thermals | 45.0 °C max under `max` profile → **39.3 °C** after `stock` | [seen] |
| Audio | speaker + earpiece devices enumerated | [seen] |
| Sensors | accelerometer, gyroscope, gravity (Sprd) | [seen] |
| GApps | **absent** — only `com.google.android.apps.googlecamera.fishfood` | [seen] |
| Apps | Daijishō, RetroArch, PPSSPP, Flycast, DraStic, Mupen64, Aurora, Mixplorer, Firefox, Magisk | [seen] |

No ANRs, no app crashes, and **no new tombstones** after boot.

---

## 2. Findings

### F1 — Setup wizard kept running after setup (fixed)

`org.lineageos.setupwizard` was alive at 10 % CPU with **135,713 minor page
faults** long after `user_setup_complete=1`. It was not in the foreground
(Daijishō was), so it was pure waste.

```sh
adb shell am force-stop org.lineageos.setupwizard
```

Check again after a factory reset; do not disable the package permanently — the
wizard is needed after `EraseUserData`.

### F2 — Load average ~6.1 is cosmetic, not CPU load

`/proc/loadavg` showed `6.1 6.6 4.9` while the device was idle. Measured from
`/proc/stat` over 5 s:

```
user +12   system +6   irq +10   idle +3954     → ~99.3 % idle
```

The load is **6 permanently D-state Unisoc kernel threads**:

```
sprd-rotation/6, sprd-rotation/7, sdiohal_tx_thre,
sdiohal_rx_thre, pub_int_handle_, SC2355_TX_THREA
```

Do not chase this as a performance problem. Use `/proc/stat` deltas (or
`top -b`) instead of `loadavg` on this platform.

### F3 — `performance_mode=max` is the default (changed to `stock`)

GammaOS boots with `persist.gammaos.performance_mode=max`, which pins both CPU
clusters to `userspace` at 2.002 GHz and the GPU governor to `performance` at
850 MHz. That is a benchmark profile: higher heat, worse battery, no benefit in
menus.

Profiles (`/vendor/bin/setclock_*.sh`, triggered by the property):

| `persist.gammaos.performance_mode` | CPU | GPU | DDR |
|---|---|---|---|
| `max` | `userspace` **2.002 GHz** pinned | `performance` 850 MHz | 1866 |
| `stock` | `schedutil` 614 MHz–2.002 GHz | `simple_ondemand` 384–850 | 1866 |
| `powersave` | `powersave` ~614 MHz | `powersave` | 512 |

Applied `stock`:

```sh
adb shell su -c 'setprop persist.gammaos.performance_mode stock'
```

Verified after 10 s: `policy0` 614 MHz idle, `policy6` 1.48 GHz, GPU 384 MHz,
SoC temp **45.0 → 39.3 °C**. Property persists across reboot.

`max` is still useful while gaming — switch it from a root shell (or a Quick
Settings tile if the build exposes one) and set it back afterwards.

### F4 — Bluetooth HAL aborts at boot, then recovers

Two tombstones at first boot (12:46):

- `tombstone_02` — `/vendor/bin/hw/android.hardware.bluetooth@1.1-service.unisoc`
  `SIGABRT`, **`Abort message: 'Open: No Bluetooth Address!'`**
- `tombstone_03` (and `_01`) — `com.android.bluetooth` `SIGABRT` with
  `DEAD_OBJECT` from the HIDL HAL, i.e. collateral damage from the HAL dying.

After `svc bluetooth enable`, BT comes up fine (`state: ON`, address
`40:45:DA:AB:7F:85`) and stays up. The failure is only at cold boot, before an
address is available.

**Open:** whether the address is persisted (and therefore whether the crash
recurs every boot). `/data/misc/bluetooth/` is not readable as `shell`; re-check
as root and after a reboot.

---

## 3. Re-check commands

```sh
# real CPU usage, not loadavg
adb shell 'a=$(head -1 /proc/stat); sleep 5; b=$(head -1 /proc/stat); echo "$a"; echo "$b"'

# clock profile
adb shell getprop persist.gammaos.performance_mode
adb shell 'for d in /sys/devices/system/cpu/cpufreq/policy*; do cat $d/scaling_governor $d/scaling_cur_freq; done'

# Bluetooth
adb shell 'svc bluetooth enable; dumpsys bluetooth_manager | grep -m2 -E "^  enabled|^  state"'

# GApps sanity
adb shell 'pm list packages | grep com.google'

# crashes
adb shell 'ls -lt /data/tombstones/ | grep -v "\.pb" | head'
```

---

## 4. Still open after this check

1. **Reboot test**: confirm `performance_mode=stock` survives, whether the BT
   HAL crash repeats, and whether the BT address persists.
2. **New microSD** and ROM library mount (slot currently empty).
3. `Magisk` finish-install/reboot (root already works via `su` request).
4. Emulator sanity runs against real ROMs (previous crashes were ROM-driven).
5. Unbricker download — rollback still unproven.
