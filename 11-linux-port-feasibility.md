# Linux on the RG405V — feasibility and port plan

**Last updated:** 2026-11-12 · **Status:** research / not started

## Verdict

**Yes, it is feasible — and the RG405V is the best-case T618 target.** The device
reports the exact board the existing mainline work uses:

```
/proc/device-tree/model      = Spreadtrum UMS512-1H10 SoC
/proc/device-tree/compatible = sprd,ums512
```

That is **`ums512_1h10`**, the reference board for the Unisoc T618 (UMS512,
sharkl5pro). `beebono/rg-rotate-linux` is a working mainline port **on the same
board name**: Linux 7.1, live DSI panel, USB dual-role OTG, Wi-Fi and Bluetooth,
booting Debian 13 from microSD through a U-Boot extlinux scan (eMMC untouched).
Anbernic's newer RG Rotate is also T618, so the SoC/board layer is shared; the
RG405V-specific work is device tree and userspace, not a from-scratch bring-up.

At the same time: **a usable Linux *emulation* ROM does not exist yet**, because
the GPU/Vulkan story is split (see §4).

## 1. What already exists

| Project | What it is | Status for RG405V |
|---|---|---|
| **GarlicOS 2.0.2** (`Black-Seraph/buildroot`) | Buildroot Linux CFW, explicitly lists **RG405M / RG405V** | Boots, but release notes say **“No HW-acceleration (yet)”** and audio stutter; 8/16-bit only |
| **`beebono/rg-rotate-linux`** | Mainline 7.1 SPRD kernel fork (off `ums9230-mainline`), U-Boot BSP fork, Debian 13 rootfs, SD-card boot | **Best base.** Same `ums512_1h10` board; panel/USB/Wi-Fi/BT proven on RG Rotate |
| **`VXTux` (Advan Tab VX Lite T1030)** | Linux 6.18.52 for UMS512/T618, **Panfrost for Mali-G52 MP2** | Useful DTS/GPU reference for the same SoC |
| **Mainline UMS512 support** | Clock, DRM KMS, SC2730 regulator, `ums512-1h10` DT (Unisoc, 2021) | Foundation is upstream |
| **`TheGammaSqueeze/UnisocBypass`** | SPL/U-Boot signature bypass, verified on UMS512/T618 | Not needed — our bootloader is already unlocked (GammaOS Next) |
| **ROCKNIX** | Rockchip-focused shared Linux core | **No T618 support and unlikely to get it** — do not wait for it |

GarlicOS is the proof that Linux boots on this exact model. `rg-rotate-linux` is
the proof that a *clean mainline* port is practical on this exact board.

## 2. Hardware inventory to port

Read from the running Android system (2026-11-12):

| Block | Evidence | Porting note |
|---|---|---|
| **SoC / board** | `sprd,ums512` · `UMS512-1H10` | Reuse `ums512-1h10` DTS as the base |
| **Display** | `card0-DSI-1`, 640×480 native, 4" IPS | DSI panel timings + backlight; rg-rotate has a `panel-generic-dsi` bring-up workspace |
| **Touch** | `goodix-ts`, i2c **3-0014**, `/dev/input/event0` | Mainline `goodix_ts` should bind with the right IRQ/reset GPIOs |
| **Buttons** | `gpio-keys`, `/dev/input/event1` | gpio-keys + `singleadc-joypad` |
| **Sticks / ADC buttons** | `singleadc-joypad` platform device | **Biggest unknown**: Unisoc ADC-joystick driver is vendor code; may need porting or an `adc-joystick` + gpio-keys replacement |
| **Fan** | `pwm-fan` + `hwmon0/pwm1`, `pwm_en`, `singleadc-joypad/fan_power` | Mainline `pwm-fan` + a `fan_power` GPIO regulator; **must be driven or the device heat-soaks** |
| **PMIC / charger** | `sc2730` (SC2730), `sprdphone-sc2730` | Regulators, charger, battery gauge; mainline SC2730 regulator exists |
| **Audio** | `sprdphone-sc2730 Headset Jack/Keyboard` | Unisoc audio codec + jack detection; usually the last block to polish |
| **Wi-Fi / BT** | Unisoc WCN over SDIO (`sprd-wlan` in Android) | rg-rotate reports working Wi-Fi/BT; reuse their WCN glue |
| **GPU** | Mali-G52 MP2 (Bifrost, “gondul”), 384–850 MHz | **Panfrost** for GLES; **PanVK is experimental**; **MaliSX2 needs kbase, not mainline Panfrost** |
| **Storage** | 128 GB eMMC + one TF slot | Boot Linux from TF; leave Android on eMMC |

## 3. Three realistic paths

### A. GarlicOS 2.x — easiest, weakest
Already supports the RG405V. Software rendering, audio stutter. Fine for
8/16-bit; not an emulation platform. Useful as a “does Linux boot?” smoke test.

### B. Mainline port from `rg-rotate-linux` — recommended
- Kernel 7.1 SPRD fork, `ums512_1h10` board, U-Boot extlinux scan, Debian 13.
- Boot from **microSD only**; eMMC Android stays intact → dual-boot by card
  presence, exactly like the RGB10 Max 2 pattern.
- Bring-up order: **console → panel → touch → fan/thermal → USB → Wi-Fi/BT →
  audio → joypad/ADC → suspend**.
- GPU: Panfrost gives GLES 3.x; RetroArch/GLES emulators work.
- **No Vulkan initially** → no PS2/GC-Vulkan, no ARMSX2 Linux.

### C. Vendor-kernel Linux + kbase + MaliSX2 — only path to Vulkan
- Reuse the Android 5.4.161 vendor kernel (or a Linux userspace on it) so
  `mali_kbase` and the vendor drivers (joypad, WCN, fan, audio) keep working.
- **MaliSX2 0.5.0** now ships an **ARM Linux** package for kbase systems
  (`malisx2/0.5.0/run-with-malisx2.sh`), which would give real Vulkan and make
  ARMSX2-on-Linux possible.
- Cost: hybrid Android-kernel userspace, no mainline DRM/Panfrost, fragile
  upgrades. This is a research track, not a clean ROM.

## 4. Base distribution: Arch-R (plan, 2026-11-12)

**Arch-R** (`archr-linux/Arch-R`) is the right *userland* base, but it is an
**RK3326** distribution — not a drop-in T618 image.

What it is: Arch Linux ARM + pacman + systemd, kernel 6.12 LTS, Mesa 26
Panfrost (libmali also shipped), a Rust/SDL2 launcher written for **640×480**
(`archr-launcher`), RetroArch + standalone cores, PortMaster, and a Docker
build (`make docker-RK3326`). Partition layout is FAT32 `BOOT` + ext4 `ROOT` +
ext4 `STORAGE` with an `extlinux.conf`/overlay boot flow.

| Keep from Arch-R | Replace for UMS512/RG405V |
|---|---|
| Arch Linux ARM rootfs, pacman, systemd | kernel: UMS512 SPRD tree (`rg-rotate` 7.1) or mainline 6.12/6.18 + UMS512 patches |
| `archr-launcher` (native 640×480) | U-Boot/boot chain: UMS512 vendor U-Boot (`ums512_1h10`) or `fastboot boot` |
| RetroArch/standalone packaging | DTS: panel, goodix touch, `singleadc-joypad`, `pwm-fan`, SC2730, WCN SDIO |
| FAT32 BOOT + ext4 ROOT/STORAGE layout | GPU stack: **Mesa Panfrost** for G52 — do **not** reuse the RK3326 libmali blob |
| zram/network/update scripts | board auto-detect via SARADC → hardcode a DTS first, adapt later |

Integration shape: fork Arch-R, add
`projects/ArchR/devices/UMS512/{linux,dts,patches,options}` and a
`make docker-UMS512` target. The rootfs is reused almost unchanged; only the
kernel, boot chain and device tree are new.

### 4.1 Booting from SD without flashing the eMMC

The stock chain is **SPL → U-Boot (`uboot_a/b`) → `boot_a` → Android**
(slot `_a`, verified-boot state `orange`, `flash.locked=0`). The SD-card
extlinux scan used by `rg-rotate` lives in **its patched U-Boot**, not in the
stock one. So "SD boot with zero writes" has to be engineered:

| Method | Writes eMMC? | Needs PC each boot? | Notes |
|---|---|---|---|
| **`fastboot boot <boot.img>`** | **no** | yes | kernel+DTB+initramfs loaded to RAM; best bring-up path |
| **kexec from rooted Android** | no | no | **not available**: vendor kernel exposes only the weak `__arm64_sys_kexec_load` stub |
| **Flash patched U-Boot to `uboot_a`** | yes (bootloader partition only) | no | what `rg-rotate` does; reversible; keep a dump |
| **Swap `boot_a` for a Linux boot image** | yes (`boot_a` only) | no | no-PC SD boot; restore the backup with fastboot |
| BootROM SD boot | — | — | UMS512 BootROM does not scan the TF slot |

Recommended sequence: **`fastboot boot` during bring-up → then `boot_a` swap
(with a verified backup) for a no-PC daily boot.** Userdata/Android are never
touched by either.

### 4.2 GPU: mainline is not GL-only

Correction to the earlier note (2026-11-12): Mesa's **PanVK supports Mali-G52
(Bifrost v7) at Vulkan 1.3** — experimental, non-conformant, and gated behind
`PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`, but real. **Panfrost GLES 3.1 is
Khronos-conformant on the G52**, and ARMSX2's release notes explicitly handle
**PanVK** as a renderer. So a mainline Arch-R image can do **both GL and
Vulkan** with one kernel.

**MaliSX2 is different:** its 0.5.0 Linux package needs Arm's **kbase** driver,
and the driver implements only what ARMSX2 calls ("every other entry point logs
and fails"). It is a **targeted PS2 accelerator**, not a general Vulkan/GL
stack — no Zink, no general apps.

Plan: mainline + Panfrost + PanVK is the primary profile. A vendor-kbase
profile (MaliSX2) is a fallback only if PanVK fails the PS2 performance gate.
Full detail: `14-gpu-acceleration-plan.md`.

## 5. What “working” will and will not mean

| Capability | Mainline (B) | Vendor kbase (C) |
|---|---|---|
| Boot to console / panel / touch | yes, likely quickly | yes |
| Fan + thermal | yes (`pwm-fan`) | yes (vendor) |
| Wi-Fi / BT | yes (rg-rotate proven) | yes |
| Audio | eventually | yes |
| GPU GLES (RetroArch, most cores) | **Panfrost** | Mali blob / MaliSX2 |
| Vulkan (PS2, GC, Vita3K) | experimental PanVK only | **MaliSX2 0.5.0** |
| Suspend / battery gauge | bring-up work | vendor |
| Risk to Android | none (SD boot) | none (SD boot) |

**Realistic expectation:** a mainline ROM is a good RetroArch/GLES machine
(PS1, SNES, N64, Saturn, DS, PSP). It is **not** a PS2 machine unless path C
works, and even then the T618 CPU is the limit.

## 6. Effort and risk

- **Console + panel + touch:** 1–2 weekends if the rg-rotate kernel and DTS
  carry over.
- **Controls (singleadc joypad), fan, Wi-Fi/BT, audio:** the bulk of the work;
  weeks, with the ADC joypad as the main unknown.
- **Polished daily ROM (suspend, charging, audio routing):** months.
- **Risk:** do **not** flash the eMMC. Boot from TF (U-Boot extlinux or
  `fastboot boot` with a kernel+DTB+initramfs). Keep the GammaOS package and the
  verified SPD unbricker as recovery (`RECOVERY.md`).
- **Architecture:** a T618 Linux build does **not** share the ROCKNIX core used
  by the RGB10/RG353V. If pursued, record it as a separate ADR (proposed
  ADR-011) so the “shared core” decision stays honest.

## 7. Recommended next step

1. Back up the current eMMC/Android state and confirm the SPD unbricker path.
2. Clone `rg-rotate-linux` with submodules; build the kernel + `ums512-1h10`
   DTB; **`fastboot boot`** it with an initramfs and USB console (no eMMC write).
   Fork Arch-R in parallel and add the `UMS512` device profile.
3. Add RG405V DTS deltas one block at a time (panel → touch → fan → joypad).
4. Only after console/panel/controls work, choose the rootfs (Debian 13 vs
   Buildroot) and the GPU path (Panfrost vs vendor kbase + MaliSX2).

## 8. Sources

- `beebono/rg-rotate-linux` — mainline 7.1 on UMS512/T618, `ums512_1h10`,
  SD-card boot, panel/USB/Wi-Fi/BT working.
- `archanaberry/advan-vxtux-6.18.52` — UMS512/T618, Panfrost for Mali-G52 MP2.
- `Black-Seraph/buildroot` GarlicOS 2.0.2 — lists RG405V; “No HW-acceleration
  (yet)”.
- Unisoc mainline patches (2021): UMS512 clock, DRM KMS, SC2730 regulator,
  `ums512-1h10` DT.
- `TheGammaSqueeze/UnisocBypass` — UMS512/T618 boot-chain analysis.
- `bmdhacks/malisx2` v0.5.0 — ARM Linux kbase package (`run-with-malisx2.sh`).
- GammaOS fan control: `/vendor/etc/init/init.gammaos_fan.rc`,
  `/vendor/bin/setfan_{off,auto,cool,max}.sh`.
