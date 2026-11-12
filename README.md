# Anbernic RG405V — Internals (Android)

Knowledge base for the **Anbernic RG405V** (Unisoc T618). **GammaOS Next v1.1
Lite (Android 14 / LineageOS 21) installed 2026-11-11**; bootloader unlocked,
Magisk 29.0, no GApps. Part of the three-device plan in `../MASTER-PLAN.md`;
shared techniques in `../SHARED-KNOWLEDGE.md`.

Evidence tags: **[seen]** observed here · **[inferred]** public docs/reasoning ·
**[unknown]** not probed.

## Device identity

| Item | Value | Evidence |
|---|---|---|
| SoC | Unisoc Tiger **T618** (2× Cortex-A75 @2.0 GHz + 6× A55), `ums512` | [seen] |
| GPU | Mali-G52, GLES 3.2, devfreq 384–850 MHz | [seen] |
| RAM | 4 GB LPDDR4X (3,623,772 kB visible), zram 2.14 GB | [seen] |
| Storage | **116.5 GB eMMC** + **128 GB microSD** (ROM library) | [seen] |
| Display | 4″ IPS touch, **480×640 portrait** panel; UI runs 640×480 landscape (rotation 1) | [seen] |
| Battery | 5500 mAh; 100 % = 3.935 V, health Good | [seen] |
| Connectivity | Wi-Fi 5 GHz (Unisoc **SC2355** SDIO), BT combo, 390/433 Mbps at probe | [seen] |
| Stock OS (replaced) | **Android 12**, build **V1.23**, security patch 2022-07-05, full GApps | [seen] |
| Bootloader / root | **UNLOCKED** (`flash.locked=0`, `verifiedbootstate=orange`, since 2026-11-11); **Magisk 29.0**, SELinux **Permissive** | [seen] |
| Custom OS (running) | **GammaOS Next v1.1 Lite** — Android **14** / LineageOS 21, `lineage_arm64_bvN-userdebug AP2A.240905.003`, no GApps | [seen] |
| Linux | No community/mainline T618 handheld port known | [inferred] |

> ⚠️ Anbernic's own product page warns that installing a third-party OS may make
> it impossible to flash back to the official OS. **Back up every partition and
> prove a rollback before the first write.** [inferred]

## Goals

1. **Highest feasible Android** — GammaOS Next (13/14) with a verified rollback.
2. **andr36oid layer parity** — performance floors, thermal control, zram,
   debloat, launcher, wireless ADB, USB gadget modes, input remaps,
   Tailscale + Moonlight.
3. **Best emulation** — AetherSX2/ARMSX2 (PS2), Dolphin (GC/Wii), PPSSPP,
   Yabasanshiro, Winlator/box64 where useful — measured per title.
4. **Documentation** — full hardware/firmware profile before any modification.

## Status

> **PS2 custom driver (2026-11-12):** **MaliSX2 0.4.1 works on the G52** via a
> patched ARMSX2 Nightly + **Vulkan** (CvS2 verified). See
> [`10-emulator-gpu-drivers.md`](10-emulator-gpu-drivers.md) for the exact
> reproduction and the other-emulator matrix.


> **2026-11-11: GammaOS Next v1.1 Lite is installed and booting.** Sequence:
> unlock (Linux `unisoc-unlock`) → `adb reboot fastboot` (fastbootd) →
> `FlashPartitions.sh` (exit 0, 0 failures) → `EraseUserData.sh` → first boot in
> ~2 min. Android **14**, no GApps, **Magisk 29.0**. Unbricker downloaded and verified 2026-11-12 (2,549,946,021 bytes, SHA-256 `9212eec4274da0bd…`)
> **not** downloaded (see below). Runbooks:
> [07-bootloader-unlock-lock-unbrick.md](07-bootloader-unlock-lock-unbrick.md) ·
> [03-gammaos-next-plan.md](03-gammaos-next-plan.md).

- [seen] Public specs collected (this file).
- [seen] Device state: **GammaOS Next v1.1 Lite (Android 14), bootloader
  unlocked, Magisk 29.0, no GApps, ADB over USB**.
- [seen] **Live recon complete** (stock era): identity/build V1.23, 77 A/B
  partitions, 116.5 GB eMMC, 128 GB microSD ROM library, T618 sysfs, panel
  480×640 portrait (UI 640×480), input devices, battery/thermals, SC2355 Wi-Fi.
  Full data: [02-recon-results.md](02-recon-results.md).
- [x] **Bootloader unlocked** (2026-11-11) — procedure, traps and verification
  in [07-bootloader-unlock-lock-unbrick.md](07-bootloader-unlock-lock-unbrick.md).
  Internal storage was wiped; user waived the `/sdcard` backup.
- [x] **GammaOS Next Lite v1.1 flashed** (2026-11-11): archive `7z t` OK → 7
  images (4.2 GB) → both slots + logical `system`/`vendor` → erase userdata.
  Results in [03-gammaos-next-plan.md](03-gammaos-next-plan.md).
- [x] **Kernel/DT/driver deep dive** (stock era): kernel 5.4.161 config, full
  running DTS, PMIC/charger/panel/touch/WiFi/GPU topology (`recon/20261108/`).
  Kernel may differ on GammaOS Next — re-verify.
- [x] **Performance tuning** applied on stock (`06-optimization.md`); **void
  after the wipe**, kept as a checklist to re-apply on GammaOS Next.
- [x] **Post-flash health check** (2026-11-11): all subsystems OK; setup wizard
  force-stopped; `performance_mode` changed `max` → **`stock`** (idle 614 MHz /
  384 MHz GPU, SoC 45.0 → 39.3 °C); Wi-Fi + BT up, no GApps, no new crashes.
  Details: [08-post-flash-health-check.md](08-post-flash-health-check.md).
- [x] **Mods built and tested (2026-11-11):** mouse mode with configurable
  speed/deadzone/AB mapping (L1+R1+Select), USB mode switcher, **USB HID
  gamepad** (kernel `F_HID`), and the **andr36oid Panel** settings APK.
  Details: [09-mods-and-tuning.md](09-mods-and-tuning.md).
- [x] **microSD validated:** `f3write` + `f3read` over the full 118 GB →
  **0 errors**, 47/84 MB/s. 5.75 GiB test set (7 systems + BIOS) copied and
  SHA-256 verified.
- [x] **D-pad in Android UI: native, row-by-row** — A opens, B goes back.
  The Tab bridge written during diagnosis was the cause of a 2-row jump (hat
  already moved one row + injected Tab); it is retired and its autostart
  removed. Source kept in `tools/dpadkeys/` for reference. §5.
- [x] **Touchscreen recovery documented:** controller can latch after USB/HID
  work; a plain reboot does not clear it, a **full power-off** does. §8.
- [x] Unbricker downloaded/verified 2026-11-12 (2,549,946,021 bytes; SHA-256 `9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71`; `7z t` OK)
  from Google Drive, Windows SPD tool.
- [ ] Device setup wizard + **Magisk finish-install** (root not yet active in
  the shell: `id -u` = 2000, no `su`).
- [ ] **Daijishō / RetroArch / emulators** — not bundled in Lite; reinstall and
  re-point to the ROM library on a new microSD.
- [ ] andr36oid layer ported.
- [ ] Emulation/perf baseline recorded.

## Docs

| File | Content |
|---|---|
| [01-recon-plan.md](01-recon-plan.md) | Step-by-step probing + backup/rollback checklist |
| [02-recon-results.md](02-recon-results.md) | **Live read-only profile**: build, partitions, CPU/GPU, panel, input, battery, Wi-Fi, ROM library |
| [03-gammaos-next-plan.md](03-gammaos-next-plan.md) | GammaOS Next Lite install, unlock, backup, rollback and post-install validation |
| [04-apps-and-launcher.md](04-apps-and-launcher.md) | Installed apps (Daijishō, Droidify, Shizuku-Next), prepared updates, Chrome/browser notes |
| [05-kernel-drivers-deep-dive.md](05-kernel-drivers-deep-dive.md) | Kernel 5.4.161 config, full running DTS, PMIC/charger/display/touch/WiFi/GPU topology |
| [06-optimization.md](06-optimization.md) | Applied performance tuning: Daijishō default home, 0.5× animations, Doze exemptions, AOT compile, safe debloat + revert commands |
| [07-bootloader-unlock-lock-unbrick.md](07-bootloader-unlock-lock-unbrick.md) | **Verified** unlock (Linux `unisoc-unlock`), state verification, lock, unbrick/rollback, mode matrix, troubleshooting |
| [08-post-flash-health-check.md](08-post-flash-health-check.md) | Post-flash health check: subsystem matrix, setup-wizard spin, cosmetic D-state load, clock profiles (`max`→`stock`), Bluetooth HAL boot abort |
| [09-mods-and-tuning.md](09-mods-and-tuning.md) | **Mouse mode** (L1+R1+Select), USB modes, **USB HID gamepad**, the **andr36oid Panel** settings app, tuning applied, fan control, and the full microSD validation |
| [10-emulator-gpu-drivers.md](10-emulator-gpu-drivers.md) | **PS2 reproduction**: patched ARMSX2 + MaliSX2 on G52, driver traps, other-emulator matrix, PS3 result |
| [11-linux-port-feasibility.md](11-linux-port-feasibility.md) | **Linux on T618**: `ums512-1h10` board, rg-rotate/Arch-R bases, SD boot without eMMC writes, hardware bring-up list |
| [13-multiboot-menu.md](13-multiboot-menu.md) | **Multiboot design**: U-Boot bootmenu for Android / Linux / TWRP / rescue, SPL selection, one-shot `boot_target`, rollout phases |
| [12-recovery-and-twrp.md](12-recovery-and-twrp.md) | **Recovery interfaces**: A/B layout, fastbootd, BROM/SPD, TWRP feasibility (`EEBBK` UMS512 precedent), rescue initramfs plan |
| [tools/](tools/) | Sources: `mousemode.c`, `hidpad.c` + `hid-gadget.sh`, `controlpanel/` (APK sources), `scripts/` (service.d, usbmode, perfmode) |
| [tools/unisoc_unlock_long.py](tools/unisoc_unlock_long.py) | Unlock helper with a configurable confirmation window (`UNLOCK_WAIT_MS`, default 240 s) instead of the tool's hard-coded 60 s |

## First contact checklist (completed 2026-11-08 → 2026-11-11)

1. ~~Do not flash anything yet.~~ → bootloader unlocked 2026-11-11.
2. [x] Developer options + USB debugging; `getprop` recorded.
3. [x] Partition table and readable partitions dumped (`02-recon-results.md`).
4. [x] Model/firmware confirmed (`ums512_1h10`, V1.23).
5. [x] Bootloader state + T618 tools confirmed
   (`07-bootloader-unlock-lock-unbrick.md`).
6. [x] GammaOS Next flash — **done 2026-11-11**; rollback proof still pending
   (unbricker downloaded/verified 2026-11-12).

## Useful references

- **No-root dashboard:** [`Zaruber/ohmyanbernic`](https://github.com/Zaruber/ohmyanbernic)
  — Termux web panel (CPU/RAM/storage/battery, files, processes, script runner),
  zero dependencies. Good for unrooted recon/telemetry; needs battery-optimization
  exemption + `termux-wake-lock`; local network only. See `../REFERENCES.md` §6.
- **Target OS:** GammaOS Next v1.1 (RG405M/V/RG505, Android 13/14).
- **Backup/flash:** T618 toolchain still **[unknown]** — verify `fastboot`/
  Unisoc tooling and a vendor image before writing.
