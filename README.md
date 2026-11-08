# Anbernic RG405V — Internals (Android)

Knowledge base for the **Anbernic RG405V** (Unisoc T618, Android 12 stock).
Part of the three-device plan in `../MASTER-PLAN.md`; shared techniques in
`../SHARED-KNOWLEDGE.md`.

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
| Stock OS | **Android 12**, build **V1.23**, security patch 2022-07-05, full GApps | [seen] |
| Bootloader / root | **locked** (`flash.locked=1`), no usable root (`su` setgid blocked), SELinux **Permissive** | [seen] |
| Custom OS | **GammaOS Next v1.1 (T618)** — Android 14, `v.1.1.0-ANBERNICT618` | [inferred] official repo/wiki |
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

> **Decision (2026-11-08): stay on stock Android 12 for now.** No unlock, no
> flash, no device changes. The GammaOS Next Lite plan
> ([03-gammaos-next-plan.md](03-gammaos-next-plan.md)) remains ready; the user
> has a Windows PC/VM for the unbricker when needed.

- [seen] Public specs collected (this file).
- [seen] Device state: **stock Android 12, not rooted, ADB available**
  (user-confirmed 2026-11-08).
- [seen] **Live recon complete** (read-only): identity/build V1.23, 77 A/B
  partitions, 116.5 GB eMMC, 128 GB microSD ROM library, T618 sysfs, panel
  480×640 portrait (UI 640×480), input devices, battery/thermals, SC2355 Wi-Fi.
  Full data: [02-recon-results.md](02-recon-results.md).
- [ ] Internal `/sdcard` backup (~4.8 GB) before any flash.
- [ ] Unbricker downloaded/verified (rollback gate).
- [ ] GammaOS Next Lite (Android 14) installed — plan:
  [03-gammaos-next-plan.md](03-gammaos-next-plan.md).
- [x] **Daijishō 1.5.0 installed**, 32 platforms imported (paths pending).
- [x] **Droidify 0.7.8 installed** (F-Droid client for FOSS apps).
- [x] **Shizuku-Next 14.0.33 installed and running** (shell UID, transport adb).
- [x] **Kernel/DT/driver deep dive** captured: kernel config, full running DTS,
  PMIC/charger/panel/touch/WiFi/GPU topology (`recon/20261108/`).
- [x] **Performance tuning applied** (no root): Daijishō default home, 0.5×
  animations, Doze exemptions, AOT compile of 17 apps, safe debloat
  (`06-optimization.md`).
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

## First contact checklist

1. Do **not** flash anything yet.
2. Enable developer options + USB debugging; record `getprop`.
3. Dump partition table and every partition you can read.
4. Verify the exact model/firmware (`ro.product.model`, `ro.build.display.id`).
5. Confirm bootloader unlock state and the correct flash tool for T618.
6. Then decide GammaOS Next and the rollback plan.

## Useful references

- **No-root dashboard:** [`Zaruber/ohmyanbernic`](https://github.com/Zaruber/ohmyanbernic)
  — Termux web panel (CPU/RAM/storage/battery, files, processes, script runner),
  zero dependencies. Good for unrooted recon/telemetry; needs battery-optimization
  exemption + `termux-wake-lock`; local network only. See `../REFERENCES.md` §6.
- **Target OS:** GammaOS Next v1.1 (RG405M/V/RG505, Android 13/14).
- **Backup/flash:** T618 toolchain still **[unknown]** — verify `fastboot`/
  Unisoc tooling and a vendor image before writing.
