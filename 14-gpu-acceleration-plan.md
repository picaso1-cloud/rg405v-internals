# GPU acceleration plan — mainline Panfrost + PanVK (and when to add kbase)

**Last updated:** 2026-11-12 · **Status:** plan, not measured on device

## Verdict

You do **not** have to choose between “full hardware compatibility” and GPU
acceleration. On this board the **mainline stack does both**:

| API | Driver | Mali-G52 status |
|---|---|---|
| OpenGL ES | **Panfrost** (Mesa) | **3.1, Khronos-conformant on G52** |
| OpenGL | Panfrost | 3.1 |
| Vulkan | **PanVK** (Mesa) | **1.3, experimental/non-conformant** (`PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`) |
| OpenCL | Rusticl | 3.0 |

Source: Mesa's own Panfrost table (G31/G51/**G52**/G76 → GLES 3.1, GL 3.1,
Vulkan 1.3). ARMSX2's release notes explicitly handle **PanVK** as a renderer,
and a 2026 downstream package (`mesa-panvk-bifrost`, Mesa 26.0.6) ran a
2.26M-test dEQP-VK sweep on a **Mali-G52 r1** (RK3566) at 97.65 % runnable-pass.

So the primary Linux profile is:

> **mainline kernel (rg-rotate 7.1) + Panfrost GLES + PanVK Vulkan + Arch-R rootfs**

That is one kernel, one rootfs, full mainline hardware (display, touch, joypad,
fan, Wi-Fi/BT, audio, suspend — all proven by `rg-rotate` on the same
`ums512_1h10` board), and both graphics APIs.

## Why MaliSX2 is not the “full compatibility” path

MaliSX2 0.5.0's Linux package is real and fast on G52, but:

- It needs **Arm's kbase kernel driver**, not mainline Panfrost.
- The driver **implements only what ARMSX2 calls** — “every other entry point
  logs and fails.” It is a **PS2 accelerator**, not a general Vulkan stack.
  Zink, other emulators and general apps cannot use it.
- GL on a vendor-kbase profile would need ARM's **libmali** Linux blob, whose
  version must match the vendor kbase — uncertain on UMS512, and it is an
  Android-era blob, not a clean Linux stack.

So a vendor profile would give **less** compatibility for daily use, not more.
It is a targeted fallback for PS2 performance if PanVK is not enough.

## Architecture: one rootfs, one or two kernel profiles

```
Arch-R rootfs (pacman, archr-launcher, RetroArch, PortMaster)
├─ Profile A (primary): mainline 7.1 + Panfrost (GL) + PanVK (Vulkan)
│    /lib/modules/7.1.0-sprd-ums512+
│    Mesa: gallium=panfrost, vulkan=panfrost
│    full mainline hardware
└─ Profile B (optional): vendor 5.4 + kbase + MaliSX2 (ARMSX2 only)
     /lib/modules/5.4.161
     ARMSX2 + MaliSX2 driver pack (VK_KHR_display / Wayland WSI)
```

The U-Boot menu (`13-multiboot-menu.md`) selects the profile; the rootfs and
ROMs are shared. Profile B is only worth building if Profile A fails the PS2
gate.

## Integration tasks for Profile A

1. **Kernel:** `rg-rotate` 7.1 SPRD tree already has the G52 node, Panfrost,
   sprd-drm, cpufreq/thermal, panel, touch, joypad, fan, Wi-Fi/BT, audio and
   suspend working on `ums512_1h10`. Port the RG405V panel/keys DTS.
2. **Mesa build:** `-Dgallium-drivers=panfrost -Dvulkan-drivers=panfrost`.
   Check the Arch ARM `mesa` package ships both; otherwise build a local
   package. PanVK is gated: set `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` for
   ARMSX2 only (a launcher wrapper, not globally).
3. **kmsro / display:** Panfrost has no display controller; Mesa pairs it with
   sprd-drm via **kmsro**. Verify `sprd-drm` is in Mesa's kmsro list; if not,
   add it (Mesa has a documented one-commit pattern, `cff7de4bb597e9`). This is
   the main “black screen despite working GPU” risk.
4. **WSI:** for a launcher/RetroArch on KMSDRM/GBM, use the GBM/EGL path;
   for Wayland, the compositor's GBM path. ARMSX2 on Linux can use
   `VK_KHR_display` directly.
5. **Verify the stack before building the distro:**
   - `vulkaninfo --summary` → Mali-G52, API 1.3, driver `panvk`
   - `glmark2-es2-drm` / `es2gears_drm` → real GLES rendering
   - `vkcube` (VK_KHR_display) → real Vulkan presentation
6. **ARMSX2 Linux:** install the Linux build; it picks the system Vulkan ICD
   (PanVK). If it refuses, run it with the PanVK gate env set. Test CvS2 and
   Metal Slug Anthology.

## Go / no-go gate

Run 3× at native 640×480, 10-minute soak, fan on `cool`/`max`, log temps:

| Test | Pass |
|---|---|
| `vulkaninfo` | G52, Vulkan 1.3, panvk loads |
| `glmark2-es2-drm` | renders, no GPU faults |
| ARMSX2 CvS2 | boots to title, playable |
| ARMSX2 Metal Slug | ≥ ~20 fps, stable 10 min |
| Dolphin (dev + MaliSX2 is Android-only; on Linux use PanVK) | GLES/Vulkan boots a light title |
| Thermals | ≤ 80 °C sustained, no throttle cliff |

If PanVK is unstable or under ~10 fps where the Android MaliSX2 baseline is
30, **then** add Profile B. Not before.

## Profile B (fallback, only if needed)

- Vendor 5.4 kernel + kbase; MaliSX2 Linux package (`malisx2-0.5.0-aarch64`),
  needs glibc ≥ 2.34, libdrm, libgbm; uses `VK_KHR_display` or Arm's Wayland
  WSI layer.
- **ARMSX2 only.** No general Vulkan, no Zink, GL via libmali (uncertain).
- Same rootfs, second `/lib/modules` + boot entry; U-Boot menu selects it.
- This is a dedicated PS2 profile, not a daily desktop.

## Performance expectation (before measuring)

- **Panfrost GLES** on G52 MP2 at 640×480: good for PS1/SNES/N64/DS/PSP/Saturn
  and light Dolphin; the G52 is the limit, not the driver.
- **PanVK Vulkan**: experimental; expect lower than MaliSX2 and possible
  app-specific failures. ARMSX2 support is the key data point to measure.
- **MaliSX2** remains the fastest known PS2 path on G52 (God of War Collection
  ~30 fps on an RG Vita/T618 per its release notes) — but it is Android-first;
  the Linux package is new (2026-11-12) and ARMSX2-specific.

## Risks

| Risk | Mitigation |
|---|---|
| PanVK fails on G52 | Profile B (kbase + MaliSX2) for ARMSX2 only |
| kmsro does not know sprd-drm | add the Mesa kmsro mapping; test `vkcube`/`glmark2` early |
| Panfrost GPU faults under load | start at 640×480, no MSAA; log `dmesg`; upstream fixes |
| Thermal throttling | fan `cool`/`max`; 70 °C passive trips (fan makes sustained viable) |
| Mesa package lacks PanVK | local Arch package with `-Dvulkan-drivers=panfrost` |

## Sequence

1. Build the mainline kernel + RG405V DTS; boot with a minimal initramfs.
2. Install Mesa (Panfrost + PanVK) in an Arch-R rootfs; run `vulkaninfo`,
   `glmark2-es2-drm`, `vkcube`.
3. Install ARMSX2 Linux; set the PanVK gate; run the PS2 gate.
4. Only if the gate fails, build Profile B.
5. Record results in this file and `10-emulator-gpu-drivers.md`.

## Sources

- Mesa Panfrost docs — G52: GLES 3.1, GL 3.1, Vulkan 1.3; PanVK experimental
  gate.
- `mesa-panvk-bifrost` (Mesa 26.0.6) — Mali-G52 r1, 97.65 % dEQP-VK runnable.
- ARMSX2 2.9.3 release notes — PanVK renderer support; Linux builds contain
  the same renderer work.
- `bmdhacks/malisx2` 0.5.0 — Linux package, kbase requirement, ARMSX2-only
  entry points, `VK_KHR_display`/Wayland WSI.
- `beebono/rg-rotate-linux` — Panfrost G52 node, sprd-drm, full hardware
  bring-up on `ums512_1h10`.
