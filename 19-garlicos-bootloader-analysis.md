# 19 — GarlicOS 2.0 bootloader for RG405 (Unisoc T618): how it works, and what it gives us

**Created:** 2026-11-20 · **Evidence level:** upstream source + documentation
(`github.com/GarlicOS/bootloader_anbernic_rg405`, `GarlicOS/init_template`,
retrododo.com/garlic-os-2-0) — **read, not yet run on our RG405V**.

## Why this matters

The RG405V's problem was never hardware support for Linux; it was **getting a
second OS past the stock, signature-verified Unisoc boot chain without wiping
Android**. GarlicOS 2.0 solves exactly that, and it solves it *without*
replacing the bootloader: it patches the **ramdisk that the stock boot chain
already loads** and re-signs the metadata with keys the device accepts.

That is a fundamentally different strategy from what we tried on the RG353V
(replacing the loader and hoping the SPL accepts it). On a chain-of-trust
device, **chain-load from the trusted stage** beats **replace the trusted
stage**.

## The mechanism (from `build.sh` + `dhtbsign.py`)

1. **Unpack the stock `vendor_boot.img`** with `cfig/Android_boot_image_editor`,
   overlay our own files into the ramdisk (`root.1`), repack.
2. **Rebuild `vbmeta.img`** with `avbtool` using the AOSP **test key**
   (`testkey_rsa4096.pem`, `SHA256_RSA4096`), preserving the chain partitions
   (`boot`, `vendor_boot`, `dtbo`, `vbmeta_system`, `l_modem`, `l_ldsp`,
   `l_gdsp`, `pm_sys`, `l_agdsp`, `l_cdsp`, …) with Anbernic's captured keys.
3. **Wrap the result in a Unisoc `DHTB` header** (`dhtbsign.py`): payload is
   SHA-256 hashed, then a 512-byte trailer *and* a header block are written at
   `1 MiB − 512` with magic `DHTB`, version `1`, the SHA-256, and size fields,
   and the image padded to exactly **1 MiB**. This is the container the Unisoc
   BootROM/bootloader expects for verified boot images.
4. **Install** that pair (`vendor_boot` + `vbmeta`) onto the device with the
   provided **Installer.apk**; running the installer again restores the
   original bootloader.

The patched ramdisk's job is only this: **find the SD card, mount the rootfs,
and hand off** — see the init script below.

## What the patched ramdisk does at boot (`GarlicOS/init_template/init`)

```sh
#!/system/bin/sh
mount -t f2fs -o loop /boot/boot/rootfs.f2fs /root      # rootfs as a file on exFAT
mount -o bind /boot /root/media                        # SD card visible inside
for f in dev dev/pts proc sys tmp firmware vendor/firmware lib/firmware \
         lib/modules device-resources $DEVICE_CUSTOM_ROOTFS_MOUNTS
do   mkdir -p /root/$f; mount -o bind /$f /root/$f; done
export PATH=... HOME=/root SHELL=/bin/sh
# force performance governor (needed on T310/T618/H700)
chroot /root /usr/bin/init
```

Consequences, and they are the important part:

| Property | Consequence for us |
|---|---|
| Runs on the **stock vendor kernel** (Android 5.4-ish) with vendor modules | Every vendor driver keeps working — panel, touch, joypad/ADC, fan, charger, Wi-Fi/BT. No DT port needed. |
| Rootfs is a **file on exFAT**, loop-mounted as **f2fs** | Our distro is a single image file on a card — trivial to rebuild, snapshot, and swap. **f2fs is mandatory** for the stock init. |
| Final step is `chroot /root /usr/bin/init` | Any userspace works **if** `/usr/bin/init` exists (or we ship our own init script and point it at `systemd`). |
| eMMC Android is untouched (only `vendor_boot`/`vbmeta` are swapped, reversible) | Android stays as fallback. Same "card = Linux, no card = Android" model we wanted everywhere. |
| No mainline kernel → **no panfrost/panvk** | GPU path is kbase/vendor GL, or **MaliSX2** for Vulkan (`malisx2/0.5.0`, ARM Linux/kbase package). That is doc `11-linux-port-feasibility.md` **path C**, now with a working bootloader under it. |

Boot trigger: power off, not plugged in → hold **power**; 1.5 s in, also hold
**home/back**; release both when the Anbernic logo appears.

## Discrepancy to check on the real release

- `README.md` describes the **older** layout (format exFAT, create `/boot`,
  put `init` there, *extract* the rootfs into `/boot`).
- `init_template` mounts **`/boot/boot/rootfs.f2fs`**.

Both are in circulation. When we run this on our unit, inspect the actual
release asset (`GarlicOS/buildroot` latest) and the installer APK contents
before building our rootfs image. Do not assume.

## Plan for the RG405V Arch port (uses this, no bootloader work)

1. **Backup** our current `vendor_boot` + `vbmeta` (and keep GammaOS image) —
   the installer is reversible, but own the restore path.
2. Install the GarlicOS bootloader APK on the RG405V → verify a stock GarlicOS
   SD boots (proves the chain, and gives us the exact expected layout).
3. Build **ArchLinuxARM aarch64** into an **f2fs** image (`rootfs.f2fs`):
   - `init` script variant that sets up the bind mounts and then
     `exec chroot /root /usr/lib/systemd/systemd` (or start `systemd` with
     `--system`), instead of `/usr/bin/init`;
   - `/lib/modules` is bind-mounted from the *vendor* kernel — keep the
     userspace ABI-neutral (pure ARM64, no kernel-module packages).
4. **Acceptance:** Arch login prompt on the RG405V panel, Android still boots
   with the card removed.
5. GPU: vendor GL first (fast, known-good), then MaliSX2 for Vulkan. Panfrost
   is *not* available on this path.

## Cross-device note (RG353V)

The same idea exists on RK3566 — patch the Android `boot`/`recovery` ramdisk on
the **eMMC** so the stock Rockchip chain chain-loads a Linux rootfs from SD.
It requires writing to eMMC (boot partition) and a Rockchip unbricker path, and
it is **not planned**: the RG353V already boots a full Linux (dArkOS) from TF1
without touching eMMC. Recorded only so the option is not lost.
