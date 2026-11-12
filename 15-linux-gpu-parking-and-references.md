# Linux / GPU track — PARKED, references and resume triggers

**Status:** **PARKED 2026-11-12** · Android remains the daily driver.
**Decision:** do not start the Linux port, U-Boot multiboot, or TWRP work until
the compatibility triggers in §3 are met.

---

## 1. Why park now

The blockers are **upstream maturity and time**, not a missing idea:

- **PanVK on Mali-G52 is real but experimental.** Mesa lists G52 at Vulkan 1.3,
  gated behind `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`; it is non-conformant.
  The 2022 field report on the same GPU (RK3568/Mali-G52 r1) hit GPU scheduler
  timeouts and page faults under real load.
- **MaliSX2 is not a general GPU stack.** Its Linux package needs Arm's
  **kbase** driver and implements only what ARMSX2 calls. No Zink, no general
  Vulkan, no GL for other apps.
- **The clean stack needs a kmsro ↔ sprd-drm check** (Mesa + Panfrost has no
  display controller), which is unproven on this board with userspace.
- **The full hardware path is close** (`rg-rotate` on the same `ums512_1h10`
  board has display, touch, joypad, fan, Wi-Fi/BT, audio and suspend working),
  but the RG405V-specific DTS (panel/keys) and the userspace Mesa test are
  still real work.
- **Dual-boot/TWRP adds bootloader risk** (custom U-Boot + SPL) on top of the
  GPU work. Not worth doing before the GPU path is proven.

Android today already gives PS2 (MaliSX2 0.4.1), PS1/DS/PSP/Saturn/N64 and a
working fan/thermal stack. Linux is a *project*, not a requirement.

---

## 2. What was learned (saved elsewhere)

| Doc | Content |
|---|---|
| `11-linux-port-feasibility.md` | board `ums512_1h10`, rg-rotate mainline 7.1, Arch-R as userland, SD-boot options, hardware inventory |
| `12-recovery-and-twrp.md` | A/B layout (no recovery partition), fastbootd, BROM/SPD, TWRP feasibility (EEBBK UMS512 precedent), rescue initramfs |
| `13-multiboot-menu.md` | U-Boot bootmenu design (Android/Linux/TWRP/rescue), SPL selection, one-shot `boot_target`, phase gates |
| `14-gpu-acceleration-plan.md` | Panfrost GLES 3.1 (conformant) + PanVK Vulkan 1.3 (experimental) primary; kbase + MaliSX2 fallback; kmsro risk |

Key facts to keep:

- Board is **`sprd,ums512` / `ums512-1h10`** — the same board as the working
  `rg-rotate` mainline port.
- **Panfrost GLES 3.1 is conformant on G52**; **PanVK reports Vulkan 1.3,
  experimental**.
- **MaliSX2 Linux = kbase + ARMSX2 only**; it is not a general ICD.
- **kexec is disabled** in the vendor kernel; MultiROM/DualBootPatcher cannot
  be used. The multiboot selector must be U-Boot.
- The stock chain has `uboot_a/b`, `boot_a/b`, `vendor_boot_a/b`, `init_boot`,
  `super` (virtual A/B), and **no recovery partition**.
- The RG405V **has an active fan**, controlled by `persist.gammaos.fan_mode`
  (`off`/`auto`/`cool`/`max`); default is effectively **off**.

---

## 3. Resume triggers (watchlist)

Start again only when **at least two** of these are true:

1. **PanVK G52 leaves the broken-driver gate** (no
   `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER`) or gets a Khronos conformance
   submission; Mesa release notes.
2. **A tested G52 PanVK build with real workloads** appears (dEQP + games,
   not just `vulkaninfo`), e.g. `mesa-panvk-bifrost` matures.
3. **ARMSX2 Linux lists PanVK as a tested renderer** (beyond “keeps push
   descriptors”), or MaliSX2 Linux is validated on a UMS512/kbase device.
4. **MaliSX2 becomes a general Vulkan ICD** (not ARMSX2-only) or gains a
   working GL/Zink path.
5. **An RG405V-specific Linux port or DTS appears** (panel, goodix touch,
   `singleadc-joypad`, fan) — e.g. `rg-rotate` or Arch-R adds it.
6. **Arch-R adds a UMS512 (T618) device profile**, or another Arch/Debian
   handheld image targets T618.
7. **Mainline UMS512 U-Boot bootmenu/dual-boot or kbase support lands**, or
   GarlicOS/another T618 Linux CFW gains HW acceleration.
8. **Panfrost kmsro gains sprd-drm support** upstream.

A single PanVK performance number is not enough — wait for a stable,
reproducible G52 result.

---

## 4. Reference evaluation (requested links)

| Link | What it is | Verdict | Use |
|---|---|---|---|
| [ARM-software/vulkan-sdk](https://github.com/ARM-software/vulkan-sdk) | Old **Vulkan SDK for Android** (Mali samples) | **Deprecated** — “This project is not supported”, redirects to Khronos Vulkan-Samples | Skip; only historical sample code |
| [ARM-software/libGPULayers](https://github.com/ARM-software/libGPULayers) | Vulkan **layer** framework + GPU Support / GPU Profile / GPU Timeline layers; Android + Linux; MIT | **Useful** | Profile/debug PanVK or MaliSX2 without changing apps; `lgl_android_install.py`, `lgl_mali_timeline_viewer.py` |
| [fxlin/mali](https://github.com/fxlin/mali) | Notes on hacking Mali GPUs: kbase job slots/atoms, MMU, registers, tracing; BSD-3 | **Useful** | Decode `kbase`/MaliSX2 logs (atom layout, UK, JIT zone), understand MMU/JS behaviour |
| [zhuanlan.zhihu.com/p/480270449](https://zhuanlan.zhihu.com/p/480270449) | 2022 “尝鲜 ARM Mali 开源 Vulkan 驱动 panvk” on **RK3568 / Mali-G52 r1** | **Useful historical caution** | PanVK build (`-Dgallium-drivers=panfrost -Dvulkan-drivers=panfrost`), `VK_ICD_FILENAMES`, gate env; ncnn results; scheduler timeouts/page faults |
| [blog.gitcode.com/650bcf4…](https://blog.gitcode.com/650bcf4dae03565c7df6789dd917c3aa.html) | RKNN-Toolkit2 **OpenCL on Mali-G52** issue write-up | **Low relevance** — AIGC, login-walled, OpenCL/NPU not Vulkan | Skip unless doing OpenCL/Rusticl/NPU |

### Notes from the Zhihu piece (2022, dated but concrete)

- PanVK started March 2021; Jason Ekstrand (Collabora) added compute support.
- On G52 r1 it enumerates and runs ncnn, but:
  - `fp16 packed` gives wrong results;
  - winograd is extremely slow;
  - under sustained load, Panfrost logs **GPU scheduler timeouts** and
    **page faults**, and Ctrl-C can hang.
- It solved “Vulkan exists on Linux” but was explicitly **testing only**.
  Treat any 2022 number as a lower bound for today, not a verdict.

### Notes from `fxlin/mali`

Useful vocabulary for future kbase/MaliSX2 debugging:

- **job slot / job chain (JC) / atom**: `base_jd_atom_v2` (u/k) vs
  `kbase_jd_atom` (kernel); ring buffer holds current + next.
- **AS / MMU**: up to 16 address spaces, 4-level page tables on Bifrost.
- **Tracing**: `mali_kbase_tracepoints`, timeline streams (OBJ/AUX),
  `MALI_MIDGARD_ENABLE_TRACE`.
- Maps directly to MaliSX2's log lines about **atom layout, UK 11.31,
  64-byte atoms, MEM_JIT_INIT / JIT zone**.

### Notes from `libGPULayers`

- Layers are the right way to inspect Vulkan call streams on Mali without
  patching the driver or app.
- `GPU Support` triages driver/feature problems; `GPU Profile` and
  `GPU Timeline` give API-level performance data.
- Works on Android and Linux; MIT; can be installed via
  `lgl_android_install.py` on Android.

---

## 5. If resumed — ordered plan (do not skip gates)

1. Build `rg-rotate` kernel + `ums512-1h10` DTB; boot via `fastboot boot` with
   an initramfs; get USB console + panel + touch + fan.
2. Install Mesa with `-Dgallium-drivers=panfrost -Dvulkan-drivers=panfrost`;
   prove `vulkaninfo` (G52, 1.3), `glmark2-es2-drm`, `vkcube`; resolve
   **kmsro/sprd-drm**.
3. ARMSX2 Linux with PanVK (`PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`); CvS2 +
   Metal Slug, 10-min soak, fan `cool`/`max`, ≤ 80 °C.
4. Only if PanVK fails: vendor 5.4 + kbase + MaliSX2 **as a separate profile**
   (ARMSX2 only), not the daily stack.
5. Only after the GPU path works: Arch-R rootfs/launcher, then U-Boot
   multiboot, then TWRP/rescue.

## 6. What is already safe to keep

- Android/GammaOS Next with the verified backups, unbricker, fan control and
  MaliSX2 0.4.1 setup.
- All Linux/GPU research in docs 11–14; no eMMC/U-Boot/SPL writes were made.
- The Transcend hub is unmounted and powered off, pending `chkdsk /f`.

---

## Sources

- Mesa Panfrost docs (G52 GLES 3.1, Vulkan 1.3, experimental gate).
- `mesa-panvk-bifrost` (2026) G52 r1 dEQP-VK runnable-pass report.
- ARMSX2 2.9.3 release notes (PanVK renderer support; Linux builds).
- `bmdhacks/malisx2` 0.5.0 (kbase requirement, ARMSX2-only entry points,
  `VK_KHR_display`/Wayland WSI).
- `beebono/rg-rotate-linux` README + `DEVICE-BRINGUP.md` (`ums512_1h10`,
  Panfrost G52, dual-boot Android/Linux, SD extlinux scan).
- Zhihu p/480270449 (2022 PanVK on RK3568/G52; timeouts/page faults).
- `fxlin/mali` (kbase internals), `ARM-software/libGPULayers` (Vulkan layers).
- `archr-linux/Arch-R` (Arch userland, launcher, boot layout).
