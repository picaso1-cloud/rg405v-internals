# 03 — GammaOS Next (Android 14) on the RG405V: install & rollback plan

**Date:** 2026-11-08 · **Target:** GammaOS Next **v1.1 (T618) — RG405V Lite**
(Android 14 / LineageOS 21 base) · **Mode:** fresh install only, **wipes the
device**.

Sources: GammaOS Next installation wiki (fetched 2026-11-08),
[`v.1.1.0-ANBERNICT618` release](https://github.com/TheGammaSqueeze/GammaOSNext/releases/tag/v.1.1.0-ANBERNICT618),
live recon in `02-recon-results.md`.

> **Decision (2026-11-08): stay on stock Android 12 for now.** Nothing in this
> plan has been executed. The device is stock, bootloader locked, no usable root.
> Revisit when a newer Android is actually wanted; the Windows unbricker path is
> available (user has a Windows PC/VM).

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

## 4. Unlock the bootloader (factory reset)

Current state: `ro.boot.flash.locked=1`, `verifiedbootstate=green`,
`sys.oem_unlock_allowed=0` [seen]. Enable **OEM unlocking** in Developer
options first.

```sh
adb reboot bootloader
# verify fastboot sees it
fastboot devices

# Linux/macOS path:
pip3 install unisoc-unlock
python3 -m unisoc_unlock
# On the device: confirm with the Home/Back button — NOT Volume Down.
# Wait for "Unlock bootloader success!".

fastboot reboot fastboot     # enter fastbootd (large red "fastbootd" text)
```

Alternative: Chrome tool `https://thegammasqueeze.github.io/subut-rehost/`
(Connect → Unlock → confirm on device with Home/Back).

> Unlocking **factory-resets/wipes** the device.

---

## 5. Download & prepare GammaOS Next

1. Download **all five** `RG405V_GammaOSNext_Lite_v1.1.7z.00X` parts (~2.3 GB).
2. Put them in one folder; extract `.001` with 7-Zip; enter the password if
   prompted. 7-Zip combines the parts into a folder containing
   `FlashPartitions.sh`, `EraseUserData.sh`, etc.
3. Verify the folder contains the RG405V scripts (not RG405M/RG505).

---

## 6. Flash (fastbootd)

```sh
# device must show fastbootd
fastboot devices
cd /path/to/RG405V_GammaOSNext_Lite_v1.1
sh FlashPartitions.sh          # up to ~10 min; do not disconnect
sh EraseUserData.sh            # mandatory factory reset/metadata erase
```

Then power-cycle once. First boot takes 1–3 minutes (may sit at the Anbernic
logo); provisioning installs Daijisho/RetroArch and lands on the home screen.

---

## 7. Rollback / unbrick

Official GammaOS Next "Stock Firmware / Unbricking" for **RG405V**:

- Unbricker: <https://drive.google.com/file/d/1idODOwE197mRHwMHDU_Y7bnAiWiackbN/view>
- The unbricker is normally driven by `UpgradeDownload.exe` (Windows). A Linux
  path must be verified before flashing (Windows VM or the Unisoc toolchain).
- After a successful stock restore, repeat the unlock/flash from §4.

> **Pre-flight gate:** download the unbricker and verify the tool can see the
> device **before** unlocking. If this cannot be proven, do not flash.

---

## 8. Post-install validation

- [ ] Repeat `02-recon-results.md` (build/partitions/SELinux/root/CPU/GPU).
- [ ] Panel 640×480 landscape, touch, brightness.
- [ ] All buttons/sticks (`retrogame_joypad`/`touch_joypad`), L2/R2, stick
      calibration.
- [ ] Wi-Fi 5 GHz + BT; 192.168.1.x connectivity.
- [ ] Audio speaker/headphones, vibration.
- [ ] Charging + battery; sleep/wake; thermals.
- [ ] microSD ROM library mounts; emulators see `/storage/<uuid>/...`.
- [ ] GammaOS Next Lite has no GApps; verify no `com.google.*` packages.
- [ ] Root (Magisk) status — needed for the full andr36oid layer.

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
