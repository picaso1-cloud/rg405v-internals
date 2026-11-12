# Multiboot menu: Android / Linux / recovery (UMS512)

**Last updated:** 2026-11-12 · **Status:** design, not built

## Verdict

The idea is sound, but the menu must live in the **bootloader**, not in TWRP.

- **MultiROM / DualBootPatcher are the wrong model here.** MultiROM needs a
  **kexec-hardboot** kernel; this vendor kernel exposes only the weak
  `__arm64_sys_kexec_load` stub (kexec disabled). DualBootPatcher is abandoned
  and patches Android boot images — it does not boot a Linux rootfs.
- **U-Boot already does this.** `rg-rotate-linux` has
  **“Dual-boot Android-from-eMMC / Linux-from-microSD — working”** on the same
  `ums512_1h10` board. Its vendor U-Boot does an `extlinux` scan on the SD
  (`mmc 1:1`) before the eMMC `boot_a`/`boot_b` slots, and can trampoline into
  stock U-Boot for Android. The SPL selects between them.
- **TWRP is a menu entry**, not the menu. It can flash, back up, and set the
  *next-boot* flag — but booting another OS is the bootloader's job.

So: build a **U-Boot bootmenu** with Android / Linux / recovery / fastboot
entries, and expose TWRP (or the rescue initramfs) as one of them.

## Proven boot chain to reuse

From `beebono/rg-rotate-linux`:

```
BootROM
  └─ SPL (patched: AVB/signature bypass)
       └─ custom vendor U-Boot  (ums512_1h10_defconfig)
            ├─ extlinux scan: mmc 1:1  → Linux on microSD   (Arch-R / Debian)
            └─ trampoline → stock U-Boot → boot_a/b → Android
```

Key components:

| Component | Source | Note |
|---|---|---|
| Vendor U-Boot fork | `rg-rotate` `src/u-boot` | panel init + `extlinux_diag.c` SD scan |
| U-Boot image packer | `tools/scripts/pack_unisoc_uboot.py` | packs into the stock `uboot_a.img` layout |
| SPL / signature patch | `vendor/spl-uboot-patch` | required for a modified U-Boot |
| Flash path | `spd_dump` / FDL (`write_part uboot_a`) | BROM recovery if it fails |
| Linux boot | `extlinux.conf` on FAT32 `mmc 1:1` | Arch-R layout already matches |
| Android boot | stock U-Boot trampoline | AVB state is already orange/unlocked |

## Menu design

`CONFIG_CMD_BOOTMENU` + `CONFIG_ANDROID_BOOT_IMAGE` + `CONFIG_CMD_SYSBOOT`
give everything needed:

```
*** ARMSX / RG405V boot menu ***
  Android (eMMC, slot A)
  Android (eMMC, slot B)
  Linux — Arch-R (microSD)
  Recovery — TWRP (microSD)
  Rescue — initramfs (microSD)
  Fastboot / fastbootd
  U-Boot console
  (autoboot in 5 s → Android)
```

Implementation notes:

- **Input:** volume up/down + power (U-Boot key drivers); the RG405V's
  sticks are `singleadc-joypad` in Linux but U-Boot only needs the GPIO keys.
  If U-Boot cannot read a key on this SKU, fall back to a **timed default** and
  a one-shot flag.
- **One-shot target:** TWRP/Android can write a small flag (Android `misc`
  BCB, or a file on the FAT boot partition) such as `boot_target=linux` /
  `boot_target=recovery`; U-Boot reads it, boots once, then clears it.
- **Recovery entry:** either TWRP (`boot` image on SD) or the rescue
  initramfs. Both are `bootm`-able; TWRP is optional.
- **Android entry:** use the existing stock-U-Boot trampoline rather than
  reimplementing AVB, `vendor_boot`/`init_boot`, dtbo and slot logic. The
  custom U-Boot only chooses; stock U-Boot does the Android boot.
- **Display:** the U-Boot fork already lights a panel and draws a logo; port
  the RG405V panel init (640×480 DSI, `lcd_d53e...960X544` name but 480×640
  DRM mode) before expecting a menu on screen.

## What to reuse from Arch-R

Arch-R already ships the user-facing pieces:

- FAT32 `BOOT` + ext4 `ROOT`/`STORAGE` layout
- `extlinux.conf` + panel overlay flow (`FDTOVERLAYS /overlays/mipi-panel.dtbo`)
- `archr-launcher` (Rust/SDL2, 640×480), RetroArch, PortMaster, pacman updates
- boot scripts that select a DTB per board

Its **RK3326 U-Boot/kernel are not reusable**, but its boot layout, launcher and
rootfs are. The menu itself is U-Boot config, independent of the distro.

## Phases and safety

| Phase | Action | Writes eMMC? | Gate |
|---|---|---|---|
| 0 | Dump `uboot_a/b`, `boot_a/b`, `vendor_boot_a`, `misc`; verify BROM/FDL entry and SPD unbricker | no | recovery proven |
| 1 | Build the vendor U-Boot **unmodified**; pack into the stock layout; boot it via FDL | yes (`uboot_a`) | Android still boots |
| 2 | Add `bootmenu` with Android-only entries; verify key input + panel | yes (`uboot_a`) | menu works, Android boots |
| 3 | Add Linux `extlinux` entry (Arch-R on SD) and rescue initramfs | yes (`uboot_a`) | Linux console + panel |
| 4 | Add TWRP entry and one-shot `boot_target` flag | maybe | backup/restore works |
| 5 | Optional: SPL patch for fully custom U-Boot selection | yes (SPL) | last, highest risk |

**Do not start with the SPL patch.** Phase 1–2 only replace `uboot_a`, which
BROM/FDL can rewrite. The SPL is the one write that can make the device
unreachable; it comes last, after the menu is proven.

## Effort

- Porting U-Boot panel init + key map to the RG405V: **days**
- Menu config, one-shot flag, Android trampoline checks: **days**
- TWRP as a menu entry: reuses the TWRP work in `12-recovery-and-twrp.md`
- Risk: medium once `uboot_a` is the only write; high only for the SPL step

## Sources

- `beebono/rg-rotate-linux` — README (U-Boot extlinux SD scan, `pack_unisoc_uboot.py`,
  `extlinux_diag.c`, `spd_dump` flow) and `DEVICE-BRINGUP.md` (“Dual-boot
  Android-from-eMMC/Linux-from-microSD — working … handled by the SPL now”).
- U-Boot docs: `bootmenu`, `CONFIG_ANDROID_BOOT_IMAGE`, `sysboot`/extlinux,
  `ab_select`, BCB/recovery boot.
- MultiROM wiki / XDA: kexec-hardboot requirement.
- DualBootPatcher guide (2017): abandoned, boot-image patching, no Linux rootfs.
- `archr-linux/Arch-R` — boot partition layout, `extlinux.conf`, launcher.
