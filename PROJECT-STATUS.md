# PROJECT STATUS — Handhelds (compaction checkpoint)

**Date:** 2026-11-08 · **Purpose:** complete cross-project state so context can
be compacted and work resumed from this file alone. Mirrored in the device repos.

> **Start here:** `MASTER-PLAN.md` (goals, shared architecture, roadmap) ·
> `SHARED-KNOWLEDGE.md` (reusable findings) · `docs/DECISIONS.md` ·
> `docs/RE-METHOD.md` · per-device repos below.

---

## Devices & access

| Device | SoC | OS | Access |
|---|---|---|---|
| **Powkiddy RGB10 Max 2** | RK3326, 1 GB | andr36oid (Android 11) on SD | `adb connect <ip>:5555` (192.168.0.99 / 192.168.31.190); root daemon via `/data/local/tmp/cmd.in`; **currently: new SD card pending** |
| **Anbernic RG353V** | RK3566, 2 GB | **GammaOS_Lite_v1.5** (Android 12, eMMC) + **ArkOS** (TF1) + ROMs (TF2) | Android: wireless adb `192.168.1.148:5555` (Magisk). ArkOS: `ssh ark@192.168.1.148`, password `ark`. Both currently offline |
| **Anbernic RG405V** | Unisoc T618, 4 GB | Stock **Android 12** (128 GB eMMC + microSD) | **not surveyed yet**; target = GammaOS Next 13/14 + andr36oid layer |

Host: `/home/souhail/Andr36oid` workspace. Devices are on `192.168.x` networks
that change between sessions — re-scan if connect fails.

---

## Goals (clarified 2026-11-08)

- **RGB10 Max 2 (weakest, reference implementation):** two OS cards swapped in
  its single slot — one **andr36oid Android** (upgrade to the newest *feasible*
  Android), one **bleeding-edge Linux** (shared core, emulator-first, optimized).
- **RG353V:** same **shared Linux core** on TF1 (patched for RK3566) while the
  eMMC keeps Android; TF2 = ROMs. Later: automated image builder/SD flasher for
  the community.
- **RG405V (most powerful):** Android-only focus — **GammaOS Next (13/14)** plus
  the andr36oid layer (speed controls, USB modes, launcher, remote play).
- **Cross-cutting:** reuse rockchip/PMIC/input/perf/streaming/build techniques
  across devices; document with `seen`/`inferred`/`unknown` evidence tags.
  Full detail: `MASTER-PLAN.md`.

---

## Project 1 — RGB10 Max 2: andr36oid port  ✅ feature-complete (rebuild pending)

**Production state:** v31 DTB + v32 service, validated on hardware. The test SD
card later **died** (unstable reads; BOOT FAT corrupted) — hence the rebuild
kit. Console currently has no working card.

### Done (all hardware-verified)
- **Wi-Fi from battery every cold boot**: rk817 PMIC-side latch via raw i2c
  `0xB4=0x47` (`rk817tool`). Latch survives warm reboots, resets on cold off.
- **Charging + Wi-Fi simultaneously** (stock parity): stock topology — OTG phy
  port **disabled**, dwc2 phyless with `phys` kept (defer timing), `dc_det`
  intact. Verified `+869 mA` charging with `wlan0` up.
- **Boot black screen fixed** (v27): auto sleep/wake cycle after bootanim
  (SF/HWC never enables the CRTC; DPMS cycle fixes it). Screen comes up alone.
- **L3/R3 fix** (v29): stick pushes emit BTN_THUMBL/THUMBR (317/318).
- **Right-stick remap** (v30/v31): `amux-channel-mapping = <1 0 2 3>` +
  `invert-absrx` (no invert-absry) — mouse mode cursor and emulator stick
  correct (the board muxes right-X on ch0/ch1 differently than the R36S; the
  stock `abs_*_channel` props are inert in this kernel).
- **Mouse mode**: toggle = **Select+Start** (hold ~1 s), pointer = right stick;
  props `pointer_stick=right`, `invert_ry=false`, `pointer_invert_x=false`.
- **Performance (v28)**: swappiness 100, SD readahead 512, iostats off, GPU max
  600 MHz (unlocked from 520), UI animations 0.5, background janitor (am
  kill-all every 40 s + force-stop thorium/vlc), `gamemode` script
  (CPU floor 1.008 GHz). RAM available 369→442 MB.
- **Remote play (v32)**: Tailscale (CLI daemon, tailnet `the console`
  `100.x.x.x`, autostart + keep-alive in service) + Moonlight v12.1
  (preinstalled) + Sunshine on PC. Service v32 runnable on any card.
- **Docs**: `PORT-REPORT.md`, `device-work/kernel/VALIDATION.md` (v9→v32),
  `FIRMWARE-GUIDE.md`, `OS-RESEARCH.md`, repo **rgb10max2-internals**.

### Next actions (ordered)
1. **Android card — new SD**: flash `dist/andr36oid-rgb10max2-master.img`
   (md5 `618f948d7055af948c362ff593433924`) → extend userdata to fill the card
   → restore `dist/userdata-full.tar` (apps/data/wifi) → restore
   `dist/essentials.tgz` (tailscale auth into `/data/local/tmp/ts/state/`) →
   boot → verify: Wi-Fi + charging + mouse mode + tailscale (`100.x.x.x`)
   + Moonlight hosts.
2. **Linux card (new goal)**: bring up the shared Linux core on the Max 2
   (ROCKNIX port — DTB/panel/input/power quirks) or validate an existing
   RK3326 image first. Two separate cards, swapped in the single slot.
3. Gameplay test of GPU 600 MHz + `gamemode` → keep or revert.
4. **Android upgrade research**: newest *feasible* Android (11 → 12/13 GSI on
   the RK3326 BSP); do not touch the stable card without a proven rollback.
5. Optional: kernel patch for honest battery icon while latched; deeper
   performance measurement.

### Known limits (by design)
- Cable ADB/USB device modes: phy port disabled for charging (documented trade-off).
- Battery icon may read “charging” while latched (self-sensing; % is honest).

### Artifacts (`/home/souhail/Andr36oid/dist/`)
- `andr36oid-rgb10max2-master.img` (4.7 GB, md5 618f948d…) — CLEAN rebuild from
  official base, verified (mtools).
- `andr36oid-rgb10max2-v26.img` — older working image (v26-era).
- `andr36oid-rgb10max2-clone.img` — clone of the DYING card (do not use).
- `userdata-full.tar` (1.5 GB) — full /data snapshot (apps+data+wifi).
- `essentials.tgz` (1.6 KB) — tailscale auth (`ts/state/tailscaled.state`).
- `andr36oid-20261024-1750-rk3326.img.xz` — official base release.
- Builders: `image-patcher/build-rgb10max2-image.sh` (mtools BOOT rebuild),
  `image-patcher/migrate-console-state.sh`.

---

## Project 2 — RG353V: GammaOS study + optimization  ✅ deployed

### Done
- Full study: hardware, GammaOS structure (Android 12 sargo GSI on RK3566 BSP,
  dynamic partitions, Magisk), drivers, **kernel+DTB decompiled** (`survey/boot.dts`),
  comparisons vs the RGB10 Max 2 pain list.
- **Wireless adb** root cause (adbd USB-only) + fix + **Magisk persistence**
  (`/data/adb/service.d/janitor.sh` sets `service.adb.tcp.port 5555`).
- **Optimization deployed**: `/data/local/tmp/gamemode` (CPU floor 816 MHz +
  DMC floor 780 MHz; GPU min not settable — devfreq ignores while suspended),
  `gamemode-off`, janitor (`am kill-all` every 60 s).
- PS2 on Android already works for the user (AetherSX2/ARMSX2 APK).
- Upgrade path studied: **GammaOSNext does NOT support the RG353 series**
  (deprecated GammaOS-RK3566 v1.5.1 is current); SDDiskTool recovery model +
  safest procedures documented.

### Next actions
1. **Linux core on TF1** (Project 4): flash ROCKNIX Generic, then port the
   andr36oid L3 layer (perf/janitor/Tailscale/Moonlight/input).
2. (Optional) deploy Moonlight + Tailscale on GammaOS for parity with the Max 2.
3. Android ceiling: GammaOS 12 is stable; a 13/14 GSI is an experiment only
   after a proven eMMC backup + rollback.

---

## Project 3 — RG353V: ArkOS side  ⏸ investigated

### Done
- Survey (Ubuntu 19.10, kernel 4.19.172, ark/ark SSH, governors, storage).
- **PS2-on-Linux investigation (complete record, `12-ps2-linux-investigation.md`)**:
  ARMSX2 Linux-arm64 AppImage fully starts after bundling glibc 2.39,
  libstdc++ 14, FreeType 2.13, libwayland 1.22 — but **ArkOS cannot display a
  Qt6 window** (weston 5 lacks `xdg_wm_base`; xwayland/weston die on VT
  signals; Xorg fbdev fails `mmap fbmem: Invalid argument`; sway can't get a
  VT from SSH). ES restored (`systemctl start emulationstation`).
- PS2 assets confirmed on the SD: `/roms2/ps2/*.iso` (GTA VCS, GTA SA, NFS UG,
  Crash TTR, SNK NeoGeo Collection, Twisted Metal) + BIOS
  `/roms2/ps2/bios/SCPH30004R.bin` (+ scph39001).

### Next actions
- Only via Project 4 (ROCKNIX). Keep ArkOS unchanged meanwhile.

---

## Project 4 — Modern Linux for RK3566/RG353V (ROCKNIX)  📚 documented (no action planned)

**Key finding (2026-11-08)**: ROCKNIX's RK3566 **Generic** image already provides
the modern stack the "Arch-R" idea was after — glibc **2.44**, Mesa **26.2.4**
(panfrost + panvk), kernel **7.2.11**, **sway**, EmulationStation — and ships
**ARMSX2 preinstalled**. Building a new Arch-R RK3566 port is therefore
**optional** (only for pacman/rolling packages), not needed for performance.

**Offline Stage 1 completed** (`14-rocknix-image-analysis.md`):
- Correct image identified: `Generic`, *not* `Specific` (RG353V is in the
  Generic list; Specific is X55/X35S/RG DS). Downloaded + SHA-256 verified
  (`b573c965…c775`); wrong image deleted.
- Boot chain captured: idbloader `RKNS` @ 32 KiB, SPL DT @ 0x36EC0
  (SD-first order), U-Boot 2026.01 @ 8 MiB, FAT @ 16 MiB with `FDTDIR`,
  KERNEL 7.2.11 (embedded LZO initramfs), SYSTEM squashfs, STORAGE ext4.
- U-Boot Anbernic board code: SARADC detection (RG353V = 695) + panel ID
  detection (0x3052 = v1 nv3051d, 0x3821 = v2). **This unit is v2** (GammaOS
  DTB active panel ID 0x3821).
- Userland verified: glibc 2.44, Mesa 26.2.4, sway kiosk, ES, ARMSX2
  (`/usr/share/armsx2-sa/armsx2-qt` + `start_armsx2.sh`), RG353V quirks
  (`/usr/lib/autostart/quirks/devices/Anbernic RG353V`).

### Boot model (corrected, verified behavior)
The RG353V has **two OS modes and two SD slots**:
- **TF1** = OS card → **boot priority**; a bootable TF1 card (Linux or Android)
  boots directly.
- **eMMC** = built-in OS (GammaOS Android) → boots only when TF1 has no bootable
  OS card.
- **TF2** = ROMs/media card only (never boots).

Therefore a ROCKNIX TF1 card is expected to boot directly **with Android
untouched on the eMMC**; removing the TF1 card boots Android again. The ROCKNIX
wiki's "wipe Android" note is an **unverified caveat** for some units, not this
device's normal behavior, and is not planned. No eMMC wipe / chainload is
needed.

### Stages
0. **Validate** (when resumed): flash `dist/ROCKNIX-RK3566-Generic.img` to a
   spare A2 card → put it in **TF1** → boot → verify panel/touch/audio/Wi-Fi/GL/
   ARMSX2. Keep ROMs on the TF2 card.
1. Capture boot chain — **done offline** (no further hardware capture needed for
   the normal TF1 path).
2. Arch rootfs hybrid — optional/unnecessary.
3. Full Arch-R RK3566 port — optional, only for pacman/rolling.

**Decision (2026-11-08): documentation-only for now — nothing to flash, no
device changes.** This remains as a reference for a future experiment.

Full plan: `13-archr-rk3566-plan.md` · analysis: `14-rocknix-image-analysis.md`.

---

## Project 5 — RG405V: newest Android + andr36oid layer  🆕 planned

**Device:** Unisoc T618, 4 GB, 128 GB eMMC, 4″ 640×480 touch, Android 12 stock
[inferred/public]. Full profile + recon checklist: `rg405v-internals/`.

- **Target OS:** **GammaOS Next v1.1** (Android 13/14 for RG405M/V/RG505)
  [inferred] + the andr36oid tuning layer.
- **Linux:** not planned (no mainline T618 handheld support) — investigation
  only.
- **First step:** read-only recon — identity, partitions, bootloader, sysfs,
  input, thermal, battery, Treble/slots; full backup + rollback proof.
- **Then:** install GammaOS Next, validate hardware, port the L3 layer
  (perf floors, zram, debloat, launcher, wireless ADB, USB modes, Tailscale +
  Moonlight, input remaps).
- **Caution:** Anbernic warns third-party OS may block returning to stock —
  backup first, prove rollback.
- **Status:** not surveyed; device access/firmware version needed.

---

## Repositories & upstream

| Repo | State |
|---|---|
| `picaso1-cloud/rgb10max2-internals` | RGB10 Max 2 knowledge base (10 docs + artifacts) |
| `picaso1-cloud/rg353v-internals` | RG353V knowledge base (15 docs + survey incl. ROCKNIX teardown) |
| `picaso1-cloud/rg405v-internals` | RG405V knowledge base (goals + recon plan + shared docs) @ `fee8d20` |
| `andr36oid/andr36oid_sdflasher` PR **#2** (`picaso1-cloud:rgb10max2-port`) | open, mergeable, head **`649dee6`** — docs + DTS + service + builder |
| Shared docs (mirrored) | `MASTER-PLAN.md`, `SHARED-KNOWLEDGE.md`, `PROJECT-STATUS.md`, `docs/DECISIONS.md`, `docs/RE-METHOD.md` |
| Local workspace | `/home/souhail/Andr36oid` — PORT-REPORT, VALIDATION, device-work, image-patcher, dist |

GitHub token: fine-grained, `picaso1-cloud` (push via explicit URL form;
never stored in files). PRs can't be created cross-repo with this token (user
opened #2 manually).

---

## Open decisions for the user

1. **RGB10 Max 2**: new card size/class → flash master + migrate state (Project 1 step 1).
2. **Linux core (shared):** confirm **ROCKNIX** (recommended) vs **Arch-R**
   (pacman/rolling) — this choice drives the Max 2 port and the RG353V image.
   See `MASTER-PLAN.md` §3.2 and `docs/DECISIONS.md` ADR-001.
3. **RGB10 Max 2 cards:** confirm the plan is two separate cards (Android +
   Linux) swapped in its single slot.
4. **Android ambition:** how far to push each device — Max 2 (11 → 12/13 GSI),
   RG353V (12 stable; 13/14 experiment), RG405V (**GammaOS Next 13/14**).
5. **RG405V access:** need ADB/root state, exact firmware version, and whether a
   full backup/rollback is possible before installing GammaOS Next.
6. **RG405V Linux:** keep as investigation-only (default) or attempt a T618
   port (high risk, poor upstream support)?
7. **Community release:** after the shared core is proven on two devices
   (`docs/DECISIONS.md` ADR-007) — or earlier?
8. **Priority order:** RGB10 rebuild → RG353V shared Linux core → RG405V recon
   → community builder/flasher.

---

## Latest session update (2026-11-08, post-checkpoint)

- **Goals clarified across all three devices** (RGB10 Max 2 / RG353V / RG405V):
  Android + Linux strategy, shared core, cross-device reuse. Written as
  `MASTER-PLAN.md`, `SHARED-KNOWLEDGE.md`, `docs/DECISIONS.md`,
  `docs/RE-METHOD.md`, and a new `rg405v-internals/` knowledge base.
- Read `PROJECT-STATUS.md`, resumed Project 4.
- Discovered the downloaded ROCKNIX image was the **wrong variant** (`Specific`);
  downloaded the correct **Generic** image, verified SHA-256, deleted the wrong one.
- Completed the offline ROCKNIX teardown (`14-rocknix-image-analysis.md`):
  boot chain, SPL DT (SD-first), U-Boot board/panel detection, glibc/Mesa/kernel,
  ARMSX2 preinstall, RG353V quirks.
- Documented the boot model correctly: **TF1 = OS card (boot priority), eMMC =
  fallback Android, TF2 = ROMs only**; no eMMC wipe needed for a TF1 ROCKNIX
  experiment. (Corrected an earlier overstatement based on an unverified wiki
  caveat.)
- Corrected the panel claim: unit is **v2** (ID 0x3821); mainline/u-boot/kernel
  support both v1 (0x3052) and v2.
- No hardware was touched; all devices remain offline/not surveyed.
- **RG353V hardware remains deferred** (documentation-only for now); the new
  master plan defines what to do when it resumes.
- **Next:** confirm the Linux core choice (ADR-001) + RG405V access, then
  execute Phase 1 (RGB10 rebuild/Linux card port).

---

## Quick reference

```sh
# RGB10 Max 2
adb connect <ip>:5555
adb shell 'cat > /data/local/tmp/cmd.in << "EOF"\n{ ... } > /data/local/tmp/cmd.out 2>&1\nchmod 666 /data/local/tmp/cmd.out\nEOF'
# tailscale (on device)
TS=/data/local/tmp/ts/tailscale; SK=--socket=/data/local/tmp/ts/tailscaled.sock
$TS $SK status ; $TS $SK ip -4

# RG353V GammaOS
adb -s <serial|ip:5555> shell ; su -c '/data/local/tmp/gamemode'
# RG353V ArkOS
sshpass -p ark ssh ark@<ip>   # sudo with same password
systemctl {stop,start} emulationstation
```

## Session history highlights (for context)

- RGB10 Max 2: v9→v32 iterations; the four big discoveries: rk817 latch,
  stock-topology charging (phy port), CRTC handoff black screen, right-stick
  mux remap. Test card died → clean rebuild kit + mtools BOOT pipeline.
- RG353V: GammaOS = GSI+vendor; wireless adb persistence; gamemode/janitor;
  ARMSX2-on-ArkOS blocked by display stack (full record); ROCKNIX identified as
  the modern path; Arch-R port plan written.
