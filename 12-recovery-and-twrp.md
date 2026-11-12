# Recovery interfaces and TWRP feasibility — RG405V (UMS512)

**Last updated:** 2026-11-12 · **Status:** research / not built

## Short answer

The device **has a recovery interface, but not a dedicated recovery partition**:

- **A/B + dynamic partitions** (`super`, `virtual_ab_device`), slot `_a`.
- **No `recovery` partition** — recovery is recovery-as-boot (in `boot` /
  `vendor_boot` / `init_boot`), as on modern A/B devices.
- **`fastboot`** (bootloader) and **`fastbootd`** (userspace, for logical
  partitions) are available: `ro.fastbootd.available=true`.
- **Unisoc BROM/FDL download mode** (Vol-Down + USB, or Home/Back combo) plus
  the verified **SPD unbricker** — the real last-resort recovery.
- GammaOS Next is LineageOS-based, so `adb reboot recovery` should bring up the
  **Lineage/AOSP recovery** (no touch TWRP UI).

So: a recovery *environment* exists; a **TWRP-style custom recovery does not**,
and can be built.

## Partition reality (2026-11-12)

```
boot_a / boot_b            mmcblk0p42 / 43
init_boot_a / init_boot_b  (Android 13+ generic ramdisk)
vendor_boot_a / _b         (vendor ramdisk / recovery resources)
dtb_a / dtb_b, dtbo_a / _b
uboot_a / uboot_b          (U-Boot — the SD-scan hook lives here)
super                      mmcblk0p52 (system/vendor/product/system_ext, A+B)
metadata, userdata, misc
```

No `recovery`. TWRP would therefore be **`fastboot boot`ed** (safe, no write)
or flashed to **`boot_a`/`vendor_boot_a`** with a verified backup.

## TWRP on UMS512/T618: precedent exists

`rtyutechstudio/android_device_EEBBK_P22H190-twrp` is a TWRP device tree for a
**UMS512 / T618** phone (BBK A2) and reports **fully functional** ADB, MTP,
`/data` FBE decryption, backup/restore and flashing. Two lessons:

1. **It is possible** on this SoC, including FBE decryption.
2. **The stock kernel contains an integrity check** (in `drm_open`) that panics
   when an unofficial recovery image is loaded. Their tree requires a **kernel
   patch** to bypass it. Expect the same on the RG405V — or build our own
   kernel (vendor BSP or `rg-rotate`) where the check is absent.

### What a RG405V TWRP port would need

| Piece | Detail |
|---|---|
| TWRP branch | 12.1 (Android 12) is the pragmatic base; Android 14 flags need updating |
| Device tree | `BoardConfig.mk` with `AB_OTA_UPDATES`, `TARGET_NO_RECOVERY`, super groups, slot handling |
| `twrp.fstab` | super logical partitions + `fileencryption=ice;wrappedkey;metadata_id=0` for Android 14 FBE |
| Kernel | vendor 5.4 (panel, goodix touch, `singleadc-joypad`, `pwm-fan`, USB gadget) **with the DRM integrity check patched out** |
| Blobs | KeyMint/keymaster + gatekeeper for FBE decryption; MTP/ADB |
| Placement | `fastboot boot` first; only then `boot_a`/`vendor_boot_a` with a backup |
| Limits | dynamic partitions are better flashed via fastbootd; `/data/media` is not included in a TWRP data backup |

Effort: **weeks** (device tree + kernel patch + fstab + decryption + testing),
not a weekend.

## Better first step: a rescue initramfs (“TWRP-lite”)

For this device the highest-value recovery tool is not a GUI:

> a small initramfs, booted with **`fastboot boot`** (zero eMMC writes), that
> gives an **ADB/console shell** plus `dd`, `parted`, `e2fsprogs`, `tar`, and
> the SD card mounted — enough to image and restore the whole eMMC.

Why it beats TWRP here:

- **Raw block backup** of `boot`, `uboot`, `super`, `userdata` — no FBE
  decryption needed, so it works even when keymaster does not.
- **No partition writes** until you deliberately `dd` back.
- Reuses the exact kernel/DTB work needed for the Linux port.
- Restore path: BROM/FDL + SPD unbricker if anything goes wrong.

A TWRP UI is still worth it later if you want touch-driven ZIP flashing and
file management; the rescue initramfs is the safer foundation.

## Recommended order

1. Dump `uboot_a`, `boot_a`, `vendor_boot_a`, `super` (headers at least) and
   keep them with `MANIFEST.sha256`.
2. Build the rescue initramfs; verify `fastboot boot` with console + ADB.
3. Prove a full raw backup to SD and a restore of a non-critical partition.
4. Only then consider TWRP 12.1, reusing the BBK tree and the kernel patch.
5. Keep GammaOS package + SPD unbricker as the outer recovery ring.

## Sources

- `rtyutechstudio/android_device_EEBBK_P22H190-twrp` — TWRP on UMS512/T618,
  kernel integrity-check patch, FBE decryption.
- AOSP: A/B updates, dynamic partitions, fastbootd, FBE (recovery cannot access
  DE storage without keymaster).
- `TheGammaSqueeze/UnisocBypass` — UMS512 boot-chain/signature analysis.
- `beebono/rg-rotate-linux` — vendor U-Boot / BROM recovery flow on the same
  `ums512_1h10` board.
