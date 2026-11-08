# 02 — RG405V recon results (live, read-only)

**Date:** 2026-11-08 · **Device:** Anbernic RG405V · **Connection:** wireless ADB
(paired via `adb pair 192.168.1.189:33661`, connected on `192.168.1.189:43815`)

All entries are **[seen]** from live ADB unless marked otherwise. Nothing was
modified: no settings changed, no partitions written, no reboot.

> **Port note:** the wireless-debugging *connect* port (43815) is random and
> changes after reboot; the pairing itself persists. Re-run `adb connect
> <ip>:<new-port>` after a reboot, or re-pair if needed.

---

## 1. Identity & build

| Item | Value |
|---|---|
| Model | **RG405V** |
| Manufacturer | Anbernic |
| Product / device | `ums512_1h10_Natv` / `ums512_1h10` |
| SoC | **Unisoc T618** (`ums512`, `ro.soc.manufacturer=Spreadtrum`) |
| Android | **12** (`ro.build.version.release=12`, SDK 31) |
| Build | **V1.23**, incremental `32523` |
| Fingerprint | `UNISOC/ums512_1h10_Natv/ums512_1h10:12/SP1A.210812.016/32523:user/release-keys` |
| Build date | 2023-08-11 |
| Security patch | **2022-07-05** (old) |
| First API level | 31 |
| ABI | `arm64-v8a` (+ `armeabi-v7a`, `armeabi`) |
| Kernel | **5.4.161** `#1 SMP PREEMPT Fri Aug 11 22:50:30 CST 2023 aarch64` |
| Bootloader | **locked** (`ro.boot.flash.locked=1`, `vbmeta.device_state=locked`), verified boot **green** |
| OEM unlock support | `ro.oem_unlock_supported=1`, but **`sys.oem_unlock_allowed=0`** (toggle off) |
| SELinux | **Permissive** (`getenforce`), `ro.boot.selinux` empty |
| Root | `/system/xbin/su` present (setuid), but `su` fails with `setgid failed: Operation not permitted` → **no usable root** |
| Treble / slots | `ro.treble.enabled=true`, A/B (`ro.boot.slot_suffix=_a`), dynamic partitions |

---

## 2. Storage & partitions

- eMMC: **116.5 GB** (`/dev/block/mmcblk0`, 128 GB class).
- **77 partitions**, A/B layout, dynamic `super`:
  - `super` = **5.6 GB** (system/vendor/product/odm)
  - `userdata` = **112.9 GB**
  - `boot_a/b` 64 MB, `vendor_boot_a/b` 100 MB, `init_boot` 8 MB,
    `dtb`/`dtbo` 8 MB, `vbmeta*` 1 MB, modem/DSP partitions 1–25 MB
  - `metadata` 16 MB, `cache` 100 MB, `persist` 2 MB, `prodnv` 10 MB
- Mounts: `/vendor` + `/product` **erofs (ro)**; `/data` **f2fs**; `/metadata` ext4.
- `/data`: **110 GB**, 4.8 GB used, 105 GB free.
- **microSD (TF)**: `public:179,1` UUID `EAFBBC4089760D1F`, **116 GB / 103 GB used**
  (ROM library — see §7). **Keep this card untouched; remove it before any flash.**

---

## 3. CPU / GPU / memory

| Block | Value |
|---|---|
| CPU | 8 cores, `sprd-cpufreq`, governor **schedutil** |
| Frequencies | 614.4 MHz – **2.002 GHz**; little/Big policies (`cpu0-3`, `cpu4-5`, `cpu6-7`) |
| GPU | **Mali-G52** (`GLES 3.2 v1.r34p0…`), devfreq **384–850 MHz**, `simple_ondemand` |
| DDR/scene | `scene-frequency` 256/384/512/768/1024/1333/**1866** MHz, `sprd-governor` |
| RAM | 3,623,772 kB total (~4 GB), ~2.1 GB available at probe |
| Swap | **zram0 2,246,732 kB** (2.14 GB), ~317 MB used |

Thermal zones (probe): `soc 45.5 °C`, `cputop 42.8 °C`, `gpu 42.8 °C`, `board 33.0 °C`, several `ank*` 35–41 °C; `soc` trips **70 °C / 85 °C**.

---

## 4. Display & input

| Item | Value |
|---|---|
| Panel (physical) | **480 × 640 portrait**, 60 Hz, density 160 |
| System UI mode | overridden to **640 × 480 landscape** (`rotation 1`), `user_rotation=1`, accelerometer rotation off |
| Backlight | `sprd_backlight`, max 255, current 255 |
| Touch | `goodix-ts` (multi-touch, 480×640) + `touch_joypad` |
| Gamepad | `retrogame_joypad` — all keys: `BTN_EAST/NORTH/WEST/SOUTH`, `BTN_TL/TR/TL2/TR2`, `BTN_SELECT/START`, `BTN_THUMBL/THUMBR`; axes `ABS_X/Y/Z/RZ`, `ABS_GAS/BRAKE`, `ABS_HAT0X/Y` |
| Other input | `gpio-keys` (volume/power), `sc27xx:vibrator`, headset keys |

---

## 5. Battery / power

- Battery: **100 %**, status **Discharging**, voltage **3.935 V**,
  current **−712 mA** (under probe load), temp **32.4 °C**, health **Good**.
- Power supplies: `ac`, `usb`, `battery`, `sc27xx-fgu`, `wireless`, `aw32257`.
- Charger controls exposed: `constant_charge_current`, `input_current_limit`,
  `charge_control_limit`, `charge_full`, `cycle_count` (read-only probe).

---

## 6. Wireless

- Wi-Fi: **Unisoc SC2355 SDIO** (`sc2355_sdio_wlan` + `sprd_wlan`), connected to
  **(SSID redacted)**, 5 GHz (5180 MHz), RSSI **−46 dBm**, link **390/433 Mbps**.
- `wlan0`: **192.168.1.189/24**, MAC `(MAC redacted)`.
- Bluetooth: `sprdbt_tty` / `wcn_bsp` present (Unisoc combo).

---

## 7. Software & ROM library

- **197 packages**, **28 third-party**, **38 Google packages** (Play Store/GMS/GSF
  present → stock is a **Full** build, not Lite).
- Launchers: `com.ylm.game` v1.0 (Anbernic game front-end), `com.ylm.launcher`
  v4.3, stock `com.android.launcher3`, `com.sprd.powersavemodelauncher`.
- Preinstalled emulators (versions seen): AetherSX2 `v1.4-2953`, Dolphin
  `5.0-17269`, PPSSPP `1.11.3`, DuckStation `0.1-4196`, RetroArch
  `1.10.3_GIT`, Citra `3aa03fc7e`, DraStic, Flycast, Mupen64Plus, Redream,
  Yabasanshiro, DOSBox, OpenBOR, etc.
- No Termux installed.
- Internal `/sdcard` has emulator data: `RetroArch/`, `DraStic/`, `PSP/`,
  `citra-emu/`, `duckstation/`, `yabause/`, `mupen64plus/`,
  `io.recompiled.redream/`, `mmjr-revamp/`, `RG_launcher/`, `OpenBOR/`.
- **microSD ROM library** (116 GB card, 103 GB used): `ps2` **19 GB**,
  `psp` **43 GB**, `psx` **16 GB**, `wii&ngc` **7.9 GB**, `neogeo` **2 GB**,
  `ss` **1.8 GB**, `snes` 928 MB, `openbor` 370 MB, `nes` 116 MB, plus
  CPS1/2/3, MAME, Dreamcast, Saturn, 3DS, MSX, PC Engine, etc.

---

## 8. What this means

1. **The device is stock, locked, unrooted, SELinux permissive.** GammaOS Next
   is a **fresh-install-only Android 14** path and requires **bootloader unlock
   (factory reset)** + fastbootd flash + mandatory user-data erase.
2. **The ROM library lives on the microSD**, not the eMMC: GammaOS Next wipes
   internal storage only, so the card can be removed and kept safe.
3. **Internal `/sdcard` emulator data (~4.8 GB)** should be copied to the PC (or
   the microSD) before flashing: RetroArch configs/saves, DraStic, PSP, Citra,
   DuckStation, Dolphin MMJR, etc.
4. **The stock full build has GApps**; GammaOS Next **Lite** is the right target
   for the ultra-light/no-GApps goal.
5. **No root means no full partition dump** from userspace. Rollback depends on
   the GammaOS team's RG405V unbricker; the stock V1.23 image is not on the
   device in a readable form. Back up what is accessible and keep the unbricker
   ready **before** unlocking.

---

## 9. Next steps

- [ ] Copy `/sdcard` emulator data to the PC (or microSD) — ~4.8 GB.
- [ ] Confirm the RG405V unbricker works / is downloaded and understood.
- [ ] Decide: unlock + GammaOS Next **Lite** (Android 14) now, or stay stock and
      only port the andr36oid tuning layer (limited without root).
- [ ] After flash: re-run this recon (new build/partitions), then port the
      andr36oid layer (perf floors, zram, debloat, launcher, ADB, Tailscale +
      Moonlight, input mapping).
- [ ] Optional: Termux + [`ohmyanbernic`](https://github.com/Zaruber/ohmyanbernic)
      dashboard for no-root telemetry (see `REFERENCES.md` §6) — but GammaOS
      Next Lite is already lean; use only if needed.

Full install/rollback procedure: `03-gammaos-next-plan.md`.
