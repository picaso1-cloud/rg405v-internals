# 03 — GammaOS Next (Android 14) on the RG405V: install & rollback plan

**Date:** 2026-11-08 · **Target:** GammaOS Next **v1.1 (T618) — RG405V Lite**
(Android 14 / LineageOS 21 base) · **Mode:** fresh install only, **wipes the
device**.

Sources: GammaOS Next installation wiki (fetched 2026-11-08),
[`v.1.1.0-ANBERNICT618` release](https://github.com/TheGammaSqueeze/GammaOSNext/releases/tag/v.1.1.0-ANBERNICT618),
live recon in `02-recon-results.md`.

> **Update 2026-11-11: §4 (unlock) is EXECUTED.** The bootloader is unlocked
> (`flash.locked=0`, `verifiedbootstate=orange`) and internal storage is wiped.
> §5 archive is downloading; §6 (flash) and §7 (rollback proof) are the remaining
> steps. See [07-bootloader-unlock-lock-unbrick.md](07-bootloader-unlock-lock-unbrick.md)
> for the verified unlock/lock/unbrick runbook.

---

## 1. Why GammaOS Next Lite

- Stock is **Android 12, V1.23, full GApps (38 Google packages), security patch
  2022-07-05** [seen].
- GammaOS Next for RG405V is **Android 14**, debloated, with performance/power
  profiles, button remapping, stick calibration, L2/R2 fixes, Magisk root
  (on supported builds), and optional **Lite = no Google services**.
- Goals: newest feasible Android + ultra-light + andr36oid tuning layer
  (`MASTER-PLAN.md` ADR-002/ADR-005).

---

## 2. Prerequisites (Linux PC)

- [ ] **USB-C data cable** (charge-only cables will not work).
- [ ] `adb` + `fastboot` (`android-sdk-platform-tools` / distro package).
- [ ] `7z` (`p7zip-full`) for the split archive.
- [ ] Python 3 + `unisoc-unlock`:
      `pip3 install unisoc-unlock` (or the Chrome tool
      <https://thegammasqueeze.github.io/subut-rehost/>).
- [ ] **Battery ≥ 50 %** (device currently 100 % [seen]).
- [ ] **microSD removed** before unlocking/flashing (the ROM library card must
      stay untouched).
- [ ] ~2.3 GB free for the RG405V Lite archive (5 parts).
- [ ] The **RG405V unbricker** downloaded and understood *before* unlocking.
- [ ] Patreon password if the release is still supporter-locked.

---

## 3. Backups (before touching anything)

The unlock/wipe destroys **internal** storage; the microSD is separate.

- [ ] Copy `/sdcard` emulator data to the PC (~4.8 GB) — at minimum:
      `RetroArch/`, `DraStic/`, `PSP/`, `citra-emu/`, `duckstation/`,
      `yabause/`, `mupen64plus/`, `io.recompiled.redream/`, `mmjr-revamp/`,
      `RG_launcher/`, `OpenBOR/`, `Android/data/<emulator>`.
      Example:
      ```sh
      adb pull /sdcard/RetroArch   ./rg405v-backup/
      adb pull /sdcard/DraStic     ./rg405v-backup/
      adb pull /sdcard/PSP         ./rg405v-backup/
      adb pull /sdcard/citra-emu   ./rg405v-backup/
      adb pull /sdcard/duckstation ./rg405v-backup/
      adb pull /sdcard/yabause     ./rg405v-backup/
      adb pull /sdcard/mupen64plus ./rg405v-backup/
      adb pull /sdcard/mmjr-revamp ./rg405v-backup/
      ```
- [ ] Optional: list installed APKs (`pm list packages -3`) and re-download the
      emulators after the flash (newer versions are preferable).
- [ ] The **microSD ROM library (103 GB)** stays as-is; GammaOS Next does not
      touch it if the card is removed during flashing.

> No root = no full eMMC partition dump. Rollback relies on the official
> unbricker; treat the first flash as **one-way until the unbricker is proven**.

---

## 4. Unlock the bootloader — ✅ DONE 2026-11-11

Before (stock): `ro.boot.flash.locked=1`, `verifiedbootstate=green`,
`sys.oem_unlock_allowed=0`. After: `flash.locked=0`, `verifiedbootstate=orange`.

```sh
python3 -m venv ~/.venvs/unisoc
~/.venvs/unisoc/bin/pip install unisoc-unlock

adb reboot bootloader                              # ~10–50 s to enumerate
until fastboot devices | grep -q 79783730379533; do sleep 1; done
~/.venvs/unisoc/bin/python -m unisoc_unlock unlock  # confirm with Home/Back, NOT Vol-
```

Traps found on the real device (details in `07-bootloader-unlock-lock-unbrick.md`):

- The tool waits only **60 s** for the on-screen confirmation. On timeout it
  raises `LIBUSB_ERROR_TIMEOUT` — **but the unlock is often already applied**.
  Our first run unlocked and wiped the device despite the error.
- Retrying then returns `FAIL: Bootloader can not been unlocked repeatly.` —
  that means **already unlocked**.
- The **"OEM unlocking" developer toggle is not required**: it stayed off
  (`sys.oem_unlock_allowed=0`) and the token/signature path worked.
- Right after the wipe **USB debugging is off** and the device enumerates as MTP
  (`1782:4001`); re-enable it in Developer options.
- Keep the **microSD out**: it was observed dropping the device out of fastboot.

Next mode for flashing:

```sh
fastboot reboot fastboot     # large red "fastbootd" text on screen
```

---

## 5. Download & prepare GammaOS Next

Lite build for this unit — 5 parts, **2.31 GB** total:

| Part | Size |
|---|---:|
| `RG405V_GammaOSNext_Lite_v1.1.7z.001` | 500.0 MB |
| `RG405V_GammaOSNext_Lite_v1.1.7z.002` | 500.0 MB |
| `RG405V_GammaOSNext_Lite_v1.1.7z.003` | 500.0 MB |
| `RG405V_GammaOSNext_Lite_v1.1.7z.004` | 500.0 MB |
| `RG405V_GammaOSNext_Lite_v1.1.7z.005` | 313.5 MB |

The T618 release is now **public** (unlocked 2026-01-22); archive password:

```
e70d48dd31d62a88b3c4f774072c93a04aecb4a0255bf55fe00458406172212b
```

1. Download all five parts into one folder.
2. `7z x RG405V_GammaOSNext_Lite_v1.1.7z.001` (password above) — 7-Zip combines
   the parts and produces the firmware folder.
3. Verify the folder contains the RG405V scripts (`FlashPartitions.sh`,
   `EraseUserData.sh`), not RG405M/RG505.
4. Integrity pre-check before flashing:
   `7z t RG405V_GammaOSNext_Lite_v1.1.7z.001 -p<password>`.

> Status 2026-11-11: **downloaded and verified.** 2,425,892,864 bytes across 5
> parts; `7z t` → `Everything is Ok` (86 files, 15 folders, 4.5 GB unpacked).

---

## 6. Flash (fastbootd) — ✅ DONE 2026-11-11

`adb reboot fastboot` goes **straight to fastbootd** on this build — no need for
`fastboot reboot fastboot`. Confirm before writing:

```sh
fastboot devices            # 79783730379533  fastbootd
fastboot getvar is-userspace # is-userspace: yes
fastboot getvar current-slot # a
```

```sh
cd RG405V_GammaOSNext_Lite_v1.1
bash FlashPartitions.sh     # exit 0, 0 failures, ~6 min
bash EraseUserData.sh       # ~60 s, ends with "Factory reset complete"
fastboot reboot
```

What `FlashPartitions.sh` writes (both slots where applicable):

| Step | Image | Notes |
|---|---|---|
| `vbmeta_a/b` | `vbmeta_custom.img` (1 MB) | disables verity for the custom build |
| `boot_a/b` | `boot_custom.img` (64 MB) | kernel/ramdisk |
| `dtbo_a/b` | `dtbo_custom.img` (8 MB) | device tree overlay |
| `vendor_boot_a/b` | `vendor_boot_custom.img` (100 MB) | |
| `vendor` (logical) | `vendor_custom.img` (763 MB) | 3 sparse chunks |
| `system` (logical) | `system_custom.img` (3.3 GB) | 13 sparse chunks, ~175 s |

Observed timing [seen]: `system` sparse transfer 175 s; full flash ≈ 6 min;
`EraseUserData` 59.9 s. `set -e` in the script means it aborts on the first
error — a clean `exit 0` is a meaningful pass.

First boot: ADB back after **40 s**, `sys.boot_completed=1` at ~2 min, one
internal reboot during provisioning.

Post-flash state [seen]: Android **14** / LineageOS 21
(`lineage_arm64_bvN-userdebug AP2A.240905.003`, build date 2025-11-21),
`ro.product.model=TrebleDroid vanilla`, `ro.boot.flash.locked=0`,
`verifiedbootstate=orange`, SELinux Permissive, **Magisk 29.0** installed,
**no GMS/Play Store** (`pm list packages | grep com.google` → only the vendor
camera `...googlecamera.fishfood`), 110 GB /data with 1.1 GB used.

---

## 7. Rollback / unbrick

Official GammaOS Next "Stock Firmware / Unbricking" for **RG405V**:

| Item | Value |
|---|---|
| File | `RG405V_UNBRICKER_SPD_Upgrade_Tool_R26.21.2801.7z` |
| Size | 2,549,946,021 bytes (~2.4 GiB) |
| URL | <https://drive.google.com/file/d/1idODOwE197mRHwMHDU_Y7bnAiWiackbN/view> |
| Tool | Unisoc SPD Upgrade Tool (`UpgradeDownload.exe`, Windows) [inferred] |

- The unbricker is normally driven by `UpgradeDownload.exe`. No verified Linux
  path exists; `spd_dump` is untested with this PAC. Use a Windows VM with USB
  passthrough.
- After a successful stock restore, repeat the unlock/flash from §4.
- **Status 2026-11-12: downloaded and verified** (2,549,946,021 bytes; SHA-256 `9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71`; `7z t` OK).
  fast connection). Until then, §6 is one-way.

> **Pre-flight gate:** download the unbricker and verify the tool can see the
> device **before** the first partition write. The unlock itself is reversible
> (stock still boots); a half-written flash is not.

Full procedure, download-mode notes and troubleshooting:
[07-bootloader-unlock-lock-unbrick.md](07-bootloader-unlock-lock-unbrick.md) §7.

---

## 8. Post-install validation

- [x] Build/SELinux/root present: Android 14, LineageOS 21, SELinux
      Permissive, **Magisk 29.0** installed (`com.topjohnwu.magisk`).
- [x] Panel geometry: `wm size` → **640×480**.
- [x] Storage: `/data` 110 GB, 1.1 GB used after first boot.
- [x] **No GApps**: no `com.google.android.gms`, no Play Store. The single
      `com.google.*` hit is the vendor camera stub
      (`com.google.android.apps.googlecamera.fishfood`).
- [ ] **Magisk finish-install** — `adb shell id -u` = 2000 and no `su` until the
      Magisk app completes its setup/reboot.
- [ ] Buttons/sticks (`gammapad` is running), L2/R2, stick calibration
      (GammaOS analog calibrator app is present).
- [ ] Wi-Fi 5 GHz + BT.
- [ ] Audio speaker/headphones, vibration.
- [ ] Charging, sleep/wake, thermals.
- [ ] microSD ROM library with a **new** card; emulators see
      `/storage/<uuid>/...`.
- [ ] Repeat the `02-recon-results.md` probes (partitions, CPU/GPU, kernel).

---

## 9. andr36oid layer after GammaOS Next

Only after the device is stable:

| Area | Target |
|---|---|
| Perf floors | CPU/GPU/DDR floors (`gamemode` equivalent on T618 sysfs) |
| Memory | zram/swappiness/readahead; GammaOS already tuned — measure first |
| Debloat | GammaOS Lite is already lean; remove only unused apps |
| Launcher | Daijishō (included) or Anbernic/RG launcher |
| Wireless ADB | persist via Magisk `service.d` (as on RG353V) |
| Streaming | Tailscale + Moonlight |
| Input | verify mapping; keep GammaOS remap tools |
| Monitoring | optional Termux + `ohmyanbernic` (no-root) for telemetry |

---

## 10. Open decisions

1. **Proceed with unlock + GammaOS Next Lite now?** (wipes internal data)
2. **Backup destination** for `/sdcard` data: PC (recommended) or microSD?
3. **Patreon access** — is the T618 release password available, or is it public?
4. **Unbricker path** — Windows VM available, or find a Linux Unisoc restore
   tool before flashing?
5. **Stock fallback** — do we want the stock V1.23 firmware archive as a second
   rollback (Anbernic download)?
