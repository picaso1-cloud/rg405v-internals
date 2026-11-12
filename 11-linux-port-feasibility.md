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

## 4. What “working” will and will not mean

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

## 5. Effort and risk

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

## 6. Recommended next step

1. Back up the current eMMC/Android state and confirm the SPD unbricker path.
2. Clone `rg-rotate-linux` with submodules; build the kernel + `ums512-1h10`
   DTB; boot it **read-only from TF** with an initramfs and USB console.
3. Add RG405V DTS deltas one block at a time (panel → touch → fan → joypad).
4. Only after console/panel/controls work, choose the rootfs (Debian 13 vs
   Buildroot) and the GPU path (Panfrost vs vendor kbase + MaliSX2).

## 7. Sources

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
