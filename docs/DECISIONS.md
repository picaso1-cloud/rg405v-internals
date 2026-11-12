# DECISIONS — architecture decision log

Format: **ADR-NNN · title · status · date**. Status: `proposed`, `accepted`,
`superseded`, `rejected`. Each decision states context, options, decision,
consequences. Hardware facts follow the evidence tags used across the docs
(`seen` / `inferred` / `unknown`).

---

## ADR-001 — One Linux core for all Linux-capable devices

**Status:** **accepted** (user, 2026-11-08) · 2026-11-08

**Context:** RGB10 Max 2 (RK3326) and RG353V (RK3566) should run "bleeding edge"
Linux with the best emulator compatibility, and we want to maintain **one**
core, not two forks. RG405V (T618) is Android-only by default.

**Options:**
1. **ROCKNIX** — supported on RK3566 now; RK3326 platform supported but the
   Max 2 is not listed (port needed). Modern kernel/Mesa/glibc, sway, ARMSX2.
2. **Arch-R** — Arch/pacman, kernel 6.12, Mesa 26.1; supports RGB10 (not Max 2)
   and has no RK3566 device (two ports needed).
3. **ArkOS** — works on both today, but Ubuntu 19.10 / kernel 4.19 / weston 5;
   blocks Qt6 apps and modern Mesa.
4. **Batocera** — supports Max 2 today; less bleeding-edge.

**Decision:** **ROCKNIX** as the shared core (confirmed). Port the RGB10 Max 2
device into it (DTB + panel + input + power/Wi-Fi quirks) using the v31 DTS and
hardware knowledge from `rgb10max2-internals/`. Arch-R remains plan B only if a
rolling `pacman` system is later required.

**Consequences:** medium device-port effort for the Max 2; one build system and
one tuning layer for both devices; RG353V path is already validated offline.

---

## ADR-002 — Android base per device

**Status:** accepted (device-specific bases) · 2026-11-08

**Context:** Three different SoCs; Android upgrade ceilings differ.

**Decision:**
- **RGB10 Max 2:** keep **andr36oid Android 11** as the stable base; research a
  GSI 12/13 upgrade separately. [inferred: no vendor 12+ for RK3326]
- **RG353V:** **GammaOS Lite 12** as the stable base (already optimized); a
  13/14 GSI is an experiment only after a proven rollback.
- **RG405V:** **GammaOS Next v1.1 (Android 13/14)** as the target base.
  Current state (user-confirmed 2026-11-08): **stock Android 12, not rooted**,
  ADB available → recon is read-only; backup/rollback must be arranged before
  any flash.

**Consequences:** the andr36oid L3 layer (scripts/services/tuning) is the shared
asset; OS base stays replaceable. "Newest Android" is a research track on
separate media, never an in-place jump on a daily device (ADR-005).

---

## ADR-003 — Evidence-tagged documentation + separate device repos

**Status:** accepted · 2026-11-08

**Context:** Findings must survive compaction and transfer between devices.
Unverified wiki claims caused a wrong conclusion once (ROCKNIX/eMMC "blocker").

**Decision:** every claim tagged `seen` / `inferred` / `unknown` with a source;
device knowledge in per-device repos; shared docs mirrored into each repo;
`PROJECT-STATUS.md` is the restart point; adopt leanrea's RE rules
(`docs/RE-METHOD.md`).

**Consequences:** slightly more writing per finding; far fewer regressions and
repeated investigations.

---

## ADR-004 — Two-card strategy on the RGB10 Max 2

**Status:** **accepted** (user, 2026-11-08; single slot confirmed) · 2026-11-08

**Context:** The user wants Android and Linux on the RGB10 Max 2.

**Decision:** **confirmed (2026-11-08)** — use **two separate microSD cards**
(one Android/andr36oid, one Linux) swapped in the single slot. Keep the user
state (`userdata-full.tar`, Tailscale auth) migratable between Android cards.

**Consequences:** no dual-boot loader work on the Max 2; card quality and
backup discipline are critical; the Linux card can be reflashed freely.

---

## ADR-005 — Delta-upgrade discipline for Android

**Status:** **accepted** (user-confirmed 2026-11-08: stable first, experiments separate) · 2026-11-08

**Context:** New Android versions on vendor BSPs can break camera/Wi-Fi/audio/
power and may be irreversible.

**Decision:** never upgrade a daily device without (1) a full partition backup,
(2) a verified rollback image/tool, (3) a spare card/eMMC target where possible.
Treat "newest Android" as a research track, not an automatic upgrade.

**Consequences:** safe experimentation; the stable card stays bootable.

---

## ADR-006 — RG405V is Android-first; Linux is investigation-only

**Status:** accepted · 2026-11-08

**Context:** T618 has poor mainline Linux support; no community Linux image for
the RG405V. The device's value is its Android performance.

**Decision:** target **GammaOS Next + andr36oid layer**. Only investigate Linux
if a concrete upstream effort appears.

**Consequences:** effort goes to Android tuning and emulation, not a risky
Linux port.

---

## ADR-007 — Community release after the shared core is proven on two devices

**Status:** proposed · 2026-11-08

**Context:** Building a flasher/image builder too early locks in unstable
choices.

**Decision:** ship the community builder/flasher **after** the RGB10 Max 2 and
RG353V both boot the shared Linux core and the Android layer is portable.

**Consequences:** Phase 4, not Phase 1; but the build scripts are written
reproducibly from the start.

---

## ADR-008 — RG405V target: GammaOS Next Lite (Android 14)

**Status:** **GammaOS Next v1.1 Lite installed 2026-11-11** — Android 14 /
LineageOS 21, bootloader unlocked, Magisk 29.0, no GApps · rollback proof
(unbricker) still pending

**Context:** Live recon shows stock Android 12 V1.23 with full GApps (38 Google
packages), bootloader locked, no usable root. GammaOS Next v1.1 for T618 offers
Android 14, debloated, with performance/remap tooling; Lite has no GApps.

**Decision:** target **GammaOS Next v1.1 RG405V Lite (Android 14)**, fresh
install. Hard gates before flashing: (1) internal `/sdcard` backup (~4.8 GB),
(2) **verified RG405V unbricker** and a working rollback path, (3) microSD ROM
card removed, (4) battery ≥ 50 %.

**Progress 2026-11-11:** gates 3 and 4 met (card out, 100 % battery). Gate 1
waived by user. Gate 2 **not yet met** — the RG405V unbricker
(`RG405V_UNBRICKER_SPD_Upgrade_Tool_R26.21.2801.7z`, 2.55 GB, Google Drive) was
not downloaded because of slow link; user will fetch it later.

**Bootloader unlocked (verified):** `ro.boot.flash.locked` 1 → **0**,
`ro.boot.verifiedbootstate` green → **orange**. Unlock performed on Linux with
`unisoc-unlock` (`adb reboot bootloader` → `python -m unisoc_unlock` → confirm
with Home/Back). The tool hard-codes a 60 s confirmation window; on timeout it
reports `LIBUSB_ERROR_TIMEOUT` **but the unlock is still applied**. A retry
returns `FAIL: Bootloader can not been unlocked repeatly.` — that means already
unlocked. The "OEM unlocking" developer toggle is **not** required. Full runbook:
[`07-bootloader-unlock-lock-unbrick.md`](../07-bootloader-unlock-lock-unbrick.md).

**Flash executed 2026-11-11:** archive verified (`7z t` OK, 2,425,892,864
bytes, 86 files), `FlashPartitions.sh` exit 0 with 0 failures (both slots +
logical `system`/`vendor`; system sparse transfer 175 s), `EraseUserData.sh`
ok. First boot to ADB 40 s, `sys.boot_completed` ~2 min. Result: Android 14
(`lineage_arm64_bvN-userdebug AP2A.240905.003`), model `TrebleDroid vanilla`,
SELinux Permissive, Magisk 29.0, **no GMS/Play Store**, 110 GB `/data`.

**Consequences:** internal storage is wiped; the 103 GB ROM library on the
microSD is preserved. The stock image is not recoverable from userspace (no
root), so the unbricker is the rollback. Post-install, port the andr36oid L3
layer only after the device is stable.

## ADR-010 — RG405V: patched ARMSX2 Nightly + MaliSX2 0.4.1 for PS2

**Status:** accepted · 2026-11-12

**Context:** ARMSX2 stable 2.9.3 and nightly 20261111 hide the MaliSX2 setup
step on Mali-G52 because `isMaliSX2Supported()` is false for G52. MaliSX2 0.4.1
itself supports G52/G52 r1 (v7) as experimental. Even with the driver
installed, the `Auto` renderer selects OpenGL on G52, so the Vulkan driver is
not used.

**Decision:** use a **patched ARMSX2 Nightly 20261111** that forces
`setMaliSX2Offered(true)`, install **MaliSX2 0.4.1**, set **Renderer =
Vulkan**, and keep the official nightly + stable `com.armsx2` as fallbacks.
The patch/build and reproduction steps live in
`tools/patches/armsx2-malisx2-g52/` and `10-emulator-gpu-drivers.md`.

**Consequences:** real custom Vulkan driver on G52 (CvS2 verified), but G52 is
experimental; the patched APK is debug-signed, so each official nightly needs
an uninstall + re-patch. Other emulators are untested; the same driver zip is
usable only where they expose a custom GPU driver loader. No stock/system
driver is modified; recovery remains GammaOS reflash + verified unbricker.
