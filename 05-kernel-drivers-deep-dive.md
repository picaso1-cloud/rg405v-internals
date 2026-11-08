# 05 — RG405V kernel, device-tree & drivers (deep dive)

**Date:** 2026-11-08 · **Access:** ADB shell / Shizuku-Next shell (both UID 2000).
**Nature:** read-only. No root, no writes.

> **Shizuku note:** Shizuku-Next runs as **UID 2000 (shell)** over `adb`, i.e.
> the *same* privilege as `adb shell`. It does **not** unlock root-only data
> (`/sys/firmware/fdt`, debugfs, kptr addresses, cpufreq writes). It is useful
> for granting apps ADB-level permissions, not for kernel introspection beyond
> what ADB already sees.

## 1. Kernel & build

| Item | Value | Evidence |
|---|---|---|
| Kernel | **5.4.161** `#1 SMP PREEMPT` | `uname -a` |
| Built | 2023-08-11, Android clang **12.0.5**, LLD 12 | `/proc/version` |
| Build host | `builder@dell-PowerEdge-R730XD` | `/proc/version` |
| SoC | **Unisoc UMS512 / T618**, `sprd,ums512` | DT `/base` |
| RAM | **4096 MB** (`androidboot.ddrsize=4096M`) | DT bootargs |
| Modules | **152** `=m` in config, **282** `/sys/module`, 160 lines in `lsmod` | config + lsmod |
| Module signing | **`CONFIG_MODULE_SIG_FORCE=y`** (SHA-512) | config |
| Kallsyms | `CONFIG_KALLSYMS_ALL=y`; names readable, **addresses 0** (kptr restricted) | config + `/proc/kallsyms` |
| Config | `/proc/config.gz` readable (38 KB) → artifact | pulled |
| Kernel cmdline | `/proc/cmdline` **Permission denied**, but the full `bootargs` is in DT `/chosen` | DT |

**Kernel config highlights:** `CONFIG_PREEMPT`, `BPF/BFP_JIT`, `KPROBES`,
`FTRACE`, `STRICT_KERNEL_RWX`, `RANDOMIZE_BASE`, `DM_VERITY`,
`F2FS_FS` (LZO/LZ4/ZSTD), `EROFS_FS`, `ZRAM`+`ZSMALLOC`, `CFG80211/MAC80211`,
`SECURITY_SELINUX` (runtime permissive), `DRM_SPRD` (m), `USB_MUSB_SPRD` (m),
`REGULATOR_SC2730` (m), `THERMAL`/`DEVFREQ_THERMAL`, `CPU_FREQ` schedutil.

## 2. Bootargs (DT `/chosen`, serial redacted)

```
androidboot.hardware=ums512_1h10
lcd_name=lcd_d53e6ea8966_mipi_2lane_960X544  lcd_size=640x480
androidboot.ddrsize=4096M  androidboot.ddrsize.range=[4096,5120)
androidboot.auto.efuse=T618  androidboot.auto.chipid=UMS512-AC
androidboot.pmic.chipid=2730  wifionly=1  dataonly=0
androidboot.verifiedbootstate=green  androidboot.flash.locked=1
androidboot.vbmeta.device_state=locked  androidboot.veritymode=enforcing
androidboot.slot_suffix=_a  buildvariant=user
androidboot.serialno=<redacted>
```

**Meaning:** WiFi-only SKU (`dataonly=0`), 4 GB DDR, SC2730 PMIC, locked/
verified boot, A-only slot suffix `_a` despite A/B partitions.

## 3. Hardware topology (running DT)

| Block | Driver / node | Details |
|---|---|---|
| **PMIC** | `sprd,sc2730` on SPI4 | FGU `sc2730-fgu`, regulators, RTC, TSENSOR, Type-C, vibrator, fast-charger |
| **Charger** | **AW32257** on I2C4 `0x6a` | 2 A charge, 4.2 V regulation, `otg-vbus` regulator `vddvbus` |
| **Charger mgr** | `charger-manager` | `cm-chargers="aw32257"`, `cm-fuel-gauge="sc27xx-fgu"`, JEITA tables |
| **Battery** | `simple-battery` | **5000 mAh** design, 4.2 V charge, OCV 3.4 → 4.296 V, 25 °C table |
| **Type-C** | `sprd,sc2730-typec` | CC1/CC2 calibration in efuse |
| **Vibrator** | `sprd,sc2730-vibrator` | input device `sc27xx:vibrator` |
| **Display** | `sprd,sharkl5pro-dsi-host` + `sprd,generic-mipi-panel` | DSI `20400000.dsi`, panel **480×640**, backlight `sprd,sharkl5pro-backlight` |
| **Touch** | **Goodix GT9xx** at I2C3 `0x14` | 480×640, 11 touch IDs, config blob, IRQ/reset GPIOs |
| **Gamepad** | `singleadc-joypad` (`retrogame_joypad`) | ADC mux, 4 channels, buttons `sw*`, 2 sticks, tuning values 0x190 |
| **Keys** | `gpio-keys` | volume down/up, power; wakeup sources |
| **Wi-Fi/BT** | `unisoc,marlin3lite_sdio` / `sprd,sc2355-sdio-wifi` | SDIO `71400000.sdio`, 32k clock, firmware `wcnmodem` partition, `/vendor/firmware/wcnmodem.bin` |
| **GNSS/FM** | `sprd,gnss`, `sprd,marlin3-fm` | firmware `gnssmodem.bin` |
| **GPU** | `sprd,mali-gondul` (Mali-G52) | OPP 384/512/614/768/850 MHz, DVFS scenes, cooling device |
| **USB** | `sprd,sharkl5pro-musb`, `extcon-usb-gpio` | MUSB host/peripheral, Type-C extcon |
| **Storage** | `mmc0` eMMC `DV6DMB`, `mmc1` SD `SD16G`, `mmc2` SDIO WiFi | eMMC 116.5 GB, microSD 128 GB |

### Key regulators (SC2730)

`vddcpu`, `vddgpu`, `vddcore`, `vddmem`, `vddmemq`, `vddwcn` (Wi-Fi),
`vddwifipa`, `vddusb33`, `vddvbus`, `vddemmccore`, `vddsdcore`, `vddsdio`,
`vddgen0/1`, `vdd28`, `avdd12/18`, camera/modem rails, `vddkpled`.

## 4. DT excerpts (sanitized)

**Battery:**

```
charge-full-design-microamp-hours = <0x4c4b40>;   /* 5,000,000 µAh = 5000 mAh */
constant-charge-voltage-max-microvolt = <0x419ce0>; /* 4.30 V */
voltage-min-design-microvolt = <0x340850>;          /* 3.41 V */
factory-internal-resistance-micro-ohms = <0x1fbd0>; /* 130 mΩ */
ocv-capacity-table-0 = <0x418d40 0x64 ... 0x33e140 0x00>; /* 4.296 V 100% → 3.40 V 0% */
```

**Panel:**

```
dsi@20400000 { compatible = "sprd,sharkl5pro-dsi-host"; ... }
panel { compatible = "sprd,generic-mipi-panel";
        sprd,surface-width = <0x1e0>;   /* 480 */
        sprd,surface-height = <0x280>;  /* 640 */
        reset-gpio = <...>; sprd,backlight = <...>; }
```

**Wi-Fi/BT:**

```
compatible = "unisoc,marlin3lite_sdio";
sprd,btwf-file-name = "/dev/block/platform/soc/soc:ap-apb/71400000.sdio/by-name/wcnmodem";
wlan { compatible = "sprd,sc2355-sdio-wifi"; };
sprd-mtty { compatible = "sprd,mtty"; sprd,name = "ttyBT"; };
gnss { compatible = "sprd,gnss"; };
sprd-fm { compatible = "sprd,marlin3-fm"; };
```

**GPU OPP:** `384 MHz @ 0.70 V`, `512/614 MHz @ 0.75 V`, `768/850 MHz @ 0.80 V`.

**Gamepad:** `compatible = "singleadc-joypad"`, `joypad-name =
"retrogame_joypad"`, `amux-count = 4`, `io-channels = <&saradc 2>`,
`amux-a/b/en-gpios`, `poll-interval = 4 ms`, per-axis tuning 0x190 (400%).

## 5. Thermal

- **17 thermal zones** (`soc`, `gpu`, `cputop`, `board`, `ank0–5`,
  `prometheus6/7`, `osctsen`, `outtsen`, `aw32257`, …).
- **7 cooling devices**: cluster0/1, GPU, etc.
- `soc-thmzone` trips **70 °C / 85 °C**; `step_wise` default governor.
- Live probe: SoC ~45 °C, GPU ~43 °C, battery 32.4 °C.

## 6. Storage

| Device | ID | Notes |
|---|---|---|
| `mmc0` | eMMC **DV6DMB** (CID `150100445636444d42…`) | 116.5 GB, A/B + dynamic super |
| `mmc1` | microSD **SD16G** (CID `fe34325344313647…`) | 128 GB ROM library |
| `mmc2` | SDIO **8800** | Unisoc SC2355 WiFi |

## 7. Security / root

- Bootloader **locked**, verified boot green, dm-verity **enforcing**.
- SELinux **enabled in kernel**, runtime **Permissive** (`getenforce`).
- `ro.secure=1`, `ro.debuggable=0`, user build.
- No root: `/system/xbin/su` exists but `setgid` fails.
- **Shizuku-Next** runs as shell UID 2000 over ADB; grants ADB-level
  permissions to apps only.

## 8. Clean boot log (2026-11-08) — captured from t=0

Method: USB ADB (persists across reboot), Wi-Fi/BT disabled to stop the
`sprd-wlan` flood, `adb reboot` → `adb wait-for-device` → immediate `dmesg`.
Captured at **uptime 7.65 s**, **9,686 lines**, starting at `[0.000000]`.

### Boot/probe timeline

| Time | Event |
|---|---|
| 0.000 | Linux 5.4.161 boots; `Machine model: Spreadtrum UMS512-1H10 SoC` |
| 0.108 | `smp: Brought up 1 node, 8 CPUs` |
| 0.286 | thermal governors registered (`step_wise`, `user_space`, `power_allocator`) |
| 0.470 | PMIC detected: **SC2730** (`sprd-sysdump: detect pmic is sc2730`) |
| 0.508 | `Run /init as init process` |
| 0.597–0.655 | modules: `sprd-sc27xx-spi`, `sc2730-regulator`, `nvmem-sc27xx-efuse` |
| 0.756–0.843 | `sdhci-sprd` loads; mmc0/mmc1/mmc2 controllers |
| 0.899 | microSD: **SD16G 117 GiB**, SDR104; `mmcblk1: p1` |
| 0.913–1.029 | **Goodix GT9xx** touch: driver V2.8.0.2, 480×640, 11 IDs, config v72, input0 |
| 0.960 | eMMC: **DV6DMB 116 GiB**, HS400 ES; 74 partitions + boot0/boot1/rpmb |
| 1.033 | `singleadc-joypad`: **EPROBE_DEFER (-517)** — ADC IIO not ready yet |
| 3.213 | joypad retries, still deferred (-517) → later succeeds (input4) |
| 3.398 | DPU DVFS registered |
| 3.419–3.55 | `[drm] backlight is not ready, dpu/panel probe deferred` (several) |
| 3.464–3.512 | **Mali-Gondul** (`mali_gondul.ko`) — DDK r34p0, arch 7.4.0 |
| 3.497 | `sc27xx:vibrator` input1 |
| 3.683–3.708 | **SC2730 Type-C** (`sc27xx_typec.ko`) — DRP, state 2 |
| 3.731–3.763 | **charger-manager**, **SC27xx FGU**, **AW32257 charger** (`4-006a`) |
| 3.939 | **SC2355 SDIO WiFi** module loads |
| 5.85–6.04 | Bluetooth service + `sprd-mtty` rfkill init |
| 6.02 | WCN version: MARLIN3_20A_W22.52.3 / sc2355_marlin3_lite (2022-12-28) |
| 6.219 | DSI mode `480x640@60` (two timings validated) |
| 6.44 | full loaded-module list (see `boot-probes-key.txt`) |
| 7.65 | uptime at capture; `sys.boot_completed=1` |

### Notable observations

- **Deferred probes are normal here:** panel/DPU wait for the backlight, and the
  joypad waits for the ADC IIO channel (`-517`); both succeed later.
- **SDHCI tuning warning** on the microSD (`mmc1: Got data interrupt 0x2 …`
  register dump) but the card enumerates and works (SDR104).
- **Sensorhub** loads (`sprd-sensorhub`, sipc id 6) but logs repeated
  `nwrite=-19 … timeout` — likely no physical sensors / unused hub.
- **Wi-Fi flood** was the only reason the ring buffer was lost before; with
  Wi-Fi/BT off at boot, the full log survives.
- Serial/MAC/email redacted before committing (`dmesg-clean-boot.txt`).

## 9. What is still unknown / next probes

- **Full `/sys/firmware/fdt`** is root-only; we have the **sysfs DT tree**
  (`dt-base` → DTS) which is equivalent for most nodes.
- **Module `.ko` path** probe was slow under WiFi log load; `lsmod` and
  `/sys/module/*/parameters` are captured, but the `/vendor/lib/modules`
  listing needs a quiet re-probe.
- **debugfs** (`/sys/kernel/debug`) is not usable as shell (hangs/empty).
- **Sensors:** DT has `sprd-sensorhub` (Sharkl5Pro), but no IIO devices were
  found; likely no accelerometer/gyro on this handheld — verify.
- **Panel driver name:** bootargs `lcd_name=lcd_d53e6ea8966_mipi_2lane_960X544`
  vs DT 480×640; the generic MIPI panel driver is used. Panel-specific init
  sequence is in the vendor kernel module; not needed for normal use.
- **Kernel module reverse engineering:** config + kallsyms names are available;
  addresses are hidden, module signatures enforced. A custom module would need
  the vendor signing key or a rebuilt kernel.

## 10. Artifacts (`recon/20261108/`)

| File | Content |
|---|---|
| `rg405v-running.dts` | full running device tree (sanitized serial) |
| `kernel-config.txt` | decompressed `/proc/config.gz` (6,528 lines) |
| `dmesg-filtered.txt` | first post-boot dmesg with `sprd-wlan`/`goodix` flood removed (1,080 lines) |
| `dmesg-clean-boot.txt`, `boot-probes-key.txt`, `uptime.txt` | **clean boot from t=0**, 9,686 lines (serial/MAC redacted) + key probe extract |
| `lsmod.txt`, `module-params.txt` | loaded modules + readable parameters |
| `vendor-firmware.txt` | `/vendor/firmware` listing (wcnmodem, gnssmodem, BT/WiFi configs) |
| `dmesg-boot-filtered.txt`, `lsmod-boot.txt`, `uptime.txt` | post-reboot capture (t ≥ 166 s; see §8) |
| `identity.txt`, `partitions.txt`, `mounts.txt`, `cpufreq.txt`, `devfreq.txt`, `thermal.txt`, `battery.txt`, `input.txt`, `display.txt`, `packages.txt` | earlier live profile |

## 11. Practical takeaway

- The kernel is a **5.4 Android BSP** with 152 modules, signed and mostly
  built-in; the hardware is a Unisoc UMS512/T618 platform (SC2730 PMIC +
  AW32257 charger + SC2355 WiFi + Mali-G52).
- **No mainline T618 support** is in sight, so Linux on the RG405V remains
  investigation-only (`MASTER-PLAN.md`, ADR-006). GammaOS Next (Android 14)
  remains the practical upgrade path.
- The full DT + config are now enough to understand **every rail, charger
  path, panel, touch, input and WiFi hook** for future Android tuning or a
  hypothetical Linux port.
