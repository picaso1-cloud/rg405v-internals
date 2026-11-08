# RE METHOD — reverse-engineering playbook

Adapted from [`aicodeking/leanrea`](https://github.com/aicodeking/leanrea) (MIT),
which distills the fastest plain-CLI path for shipped software. This file is our
version for **handheld firmware, Android images, Linux distros and binaries**.

The goal is not "decompile everything". It is: **answer the question with the
cheapest evidence, and record how you got it.**

---

## 1. Non-negotiable rules

1. **Work on a copy.** Dump/extract into a scratch dir (`/tmp/opencode/re/<name>`).
   Never edit the original image, partition or card.
2. **Route by `file` and magic bytes first.** Then the cheapest inventory, then
   targeted reads, then decompilation only if needed.
3. **Batch independent probes into one shell call** with `echo '--- name'`
   markers; cap output (`head`, `-m`).
4. **Slice, don't `cat`** big or minified files:
   `rg -o '.{0,200}NEEDLE.{0,200}' file | head`.
5. **Search extracted trees with `rg -uu`** — bundlers and firmwares hide code
   in dot-directories.
6. **Mark every claim**: **seen** (cite file:offset / command output / symbol),
   **inferred**, or **unknown**. A fact you did not find is *unknown*, not
   "absent". Reading code is **inferred** until the behavior is observed.
7. **A missing tool is not a blocker** — use a fallback or install it with
   permission.
8. **Turns cost more than bytes** — aim for 2–4 tool calls per question: one
   batched recon, one or two slices, then answer.

---

## 2. First moves by target

| Target | Fastest path |
|---|---|
| **Android image / card** | `fdisk -l`, `blkid`, mount read-only (or mtools for FAT), list `/system`, `/vendor`, `/boot`; `unpack_bootimg` for boot.img; `getprop` dump on device |
| **APK** | `jadx -d out app.apk` then `rg`; manifest/resources only: `apktool d --no-src`; `unzip -l` for native libs |
| **Native ELF (aarch64)** | `file`, `readelf -hlSW`, `readelf -dW` (NEEDED), `nm -D --defined-only`, `strings -a -n 6`, `objdump -d` |
| **Kernel / DTB** | `file`, `strings` for version; `dtc -I dtb -O dts`; find FDT magic `d00dfeed`; compare DTBs with `dtc` |
| **Bootloader / firmware** | `binwalk -e` or `unblob`; `strings` for `extlinux`, `boot.ini`, `Image`, `uInitrd`; map raw offsets (`idbloader` `RKNS` @ 32 KiB, u-boot @ 8 MiB on Rockchip) |
| **Squashfs / erofs rootfs** | `unsquashfs -l` / `fsck.erofs --extract`; list package DB, `/etc/os-release`, `glibc` strings, Mesa/DRI |
| **Debian/Ubuntu rootfs** | `dpkg -l`, `/var/lib/dpkg/status`, `apt-cache policy` (if bootable) |
| **Arch/pacman rootfs** | `/var/lib/pacman/local` |
| **Build system** | `README`, `projects/<distro>/devices/<SoC>/`, `packages/*/package.mk`, kernel configs, patches |
| **Website / docs** | `curl -sL`; fetch only the linked pages needed; cite URL |

Device-specific additions:

- **Rockchip:** idbloader/u-boot raw offsets, `extlinux.extlinux`/`boot.ini`,
  SARADC model detection, panel ID read, `rkdeveloptool`/maskrom.
- **Unisoc (T618):** verify bootloader/flash tool before writing; `fastboot` and
  `spd_dump`/Unisoc tool paths differ; **never assume** Android's fastboot is
  the whole story.
- **Android vendor BSP:** `/vendor/build.prop`, `/vendor/etc/init/*.rc`,
  `/vendor/lib64/hw/`, `/proc/device-tree`, `dumpsys`, `getprop`.

---

## 3. Evidence log template

Every reverse-engineering session appends a block to the device doc:

```md
### RE-YYYYMMDD-NN — <question>
- Target: <path / device / image>  (sha256: ...)
- Method: <commands run>
- Seen: <file:offset, symbol, output>
- Inferred: <reasoning>
- Unknown: <what remains>
- Decision / next probe: ...
```

This is what keeps the backlog from turning into folklore.

---

## 4. Firmware safety checklist

Before flashing or erasing anything:

1. Record model, SoC revision, firmware version, bootloader state, partition
   table (`lsblk`, `fdisk -l`, `getprop`).
2. Dump and hash every partition you may need to restore (boot, vendor, vbmeta,
   super, userdata if relevant).
3. Prove a rollback path (vendor image/tool) **before** the first write.
4. Prefer a spare SD/eMMC target. Keep the daily card untouched until the new
   image boots.
5. Verify the written image (sha256 of the raw file + a read-back of the first
   MB / FAT with mtools).

---

## 5. Speed patterns

- **Strings and imports first.** On a small binary, `nm -u` + `strings` usually
  answers "what does it do" without disassembly.
- **Find who uses a string** by locating the string's address and the enclosing
  function (objdump/otool labels literals inline).
- **Read call sequences**, not every instruction: `bl`/`call` targets already
  tell the protocol.
- **Diff inventories before diffing code** (version A vs B: files, exports,
  build info).
- **Prefer runtime evidence** when available (`logcat`, `dmesg`, `strace`,
  `frida`, `adb shell`), and mark it **seen**.

---

## 6. Credit

leanrea is MIT-licensed; this playbook reuses its rules and router ideas for a
handheld-firmware context. Keep the credit when copying further.
