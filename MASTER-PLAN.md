# MASTER PLAN — andr36oid handheld platform (3 devices)

**Date:** 2026-11-08 · **Status:** living document · **Restart point:** `PROJECT-STATUS.md`

This is the north star for the whole collection. Per-device details live in
their own repos; this file explains **why** we do things, **what is shared**,
and **what order** we do it in. Every claim is tagged:

- **[seen]** — observed on hardware / in a file / in command output (source given)
- **[inferred]** — reasoned from evidence or public docs, not yet observed here
- **[unknown]** — not found; must be probed

---

## 1. Mission

Build one coherent, reusable handheld platform instead of three unrelated
projects:

1. **Android layer ("andr36oid")** — ultra-light, no GApps, gaming/streaming
   first: debloat, performance floors, thermal/battery control, launcher, USB
   modes, remote play.
2. **Linux core ("andr36oid Linux")** — bleeding-edge, emulator-first, low
   resource: modern kernel + Mesa, ARMSX2/PS2, RetroArch/standalones, Emulation
   Station front-end, reproducible image.
3. **Per-device adaptation** — device tree, panel, input, power, Wi-Fi and
   boot-chain quirks; never a divergent fork unless unavoidable.
4. **Reproducible releases** — image builder + SD flasher + state migration,
   usable by the community.
5. **Knowledge base** — every reverse-engineered detail documented with
   evidence, so nothing is lost between sessions or devices.

---

## 2. Devices and goals

| | **Powkiddy RGB10 Max 2** | **Anbernic RG353V** | **Anbernic RG405V** |
|---|---|---|---|
| SoC | Rockchip **RK3326** (4×A35) | Rockchip **RK3566** (4×A55 @1.8) | Unisoc **T618** (2×A75+6×A55) |
| GPU | Mali-G31 | Mali-G52 | Mali-G52 MP2 @850 MHz |
| RAM | 1 GB | 2 GB | 4 GB LPDDR4X |
| Display | 5″ 480×854 (portrait panel, rotated) | 3.5″ 640×480 touch | 4″ 640×480 touch |
| Storage / boot | **single microSD** (whole OS on card) | 32 GB eMMC + **TF1** (OS) + **TF2** (ROMs) | 128 GB eMMC + microSD |
| Current OS | **andr36oid Android 11** (v31 DTB + v32 service) | eMMC **GammaOS Lite 12**; TF1 **ArkOS** | Stock **Android 12** (verify) |
| Target systems | **2 cards, swapped:** Android card + Linux card | eMMC Android + TF1 Linux + TF2 ROMs (simultaneous) | **Android only** (Linux unlikely on T618) |
| Android goal | newest **feasible** Android (11 now → 12/13 research) | 12 stable (GammaOS); 13/14 experiment only | **GammaOS Next 13/14** + andr36oid layer |
| Linux goal | shared **andr36oid Linux** core (port device) | **same core**, RK3566 patched | investigate only (no mainline T618 support) |
| Use case | light, ultra-portable; streaming + retro | daily driver; PS2/GC/Wii-class + Android apps | most powerful; Android + high-end emulation |
| Status | Android feature-complete; OS-card rebuild pending | Android optimized; ROCKNIX analyzed; Linux not flashed | **new, not surveyed** |

**Verified notes**

- RGB10 Max 2 has **one** microSD slot — the "two systems" are **two cards you
  swap**, not simultaneous. [seen: hardware + `rgb10max2-internals/01-hardware.md`]
- RG353V boot model: **TF1 OS card boots first**, eMMC is the fallback Android,
  **TF2 is ROMs-only**. [seen/user-verified 2026-11-08]
- RG405V stock is **Android 12**, T618, 4 GB, 128 GB eMMC, microSD slot.
  [inferred: anbernic.com product page; must verify on the device]
- GammaOS **Next** supports **RG405M/RG405V/RG505** (Android 13/14), while the
  RG353 series stays on GammaOS RK3566 (Android 12).
  [inferred: gammaos.org + GammaOSNext repo; verify exact build on device]

---

## 3. Shared architecture

### 3.1 Layers

```
L5  Build / release      image builder · SD flasher · verification · migration
L4  Apps / emulators     ES · RetroArch · ARMSX2 · Dolphin · PPSSPP · Moonlight
L3  andr36oid tuning     perf floors · governors · zram · debloat · launcher
                         USB modes · WiFi/ADB · Tailscale · input remaps
L2  OS base              Linux core (ROCKNIX)  |  Android base (GammaOS/Lineage)
L1  Boot chain           Rockchip idbloader/u-boot/extlinux | Unisoc bootloader
L0  Hardware knowledge   DTS · PMIC · panel · input · WiFi · thermal · battery
```

Everything below L3 is device-specific only where it must be. L3 is the part
we want to **write once and port** — it is the real "andr36oid" value.

### 3.2 Linux core — decision needed

The goal: **one Linux core** used on the RGB10 Max 2 first, then carried to the
RG353V with only board patches.

| Candidate | Max 2 support | RG353V support | Modernity | PS2/ARMSX2 | Effort | Verdict |
|---|---|---|---|---|---|---|
| **ROCKNIX** | platform (RK3326) yes, **Max 2 not listed** → port DTB/quirks [inferred] | ✅ fully supported [seen] | kernel 7.x, glibc 2.44, Mesa 26.x, sway [seen] | ✅ preinstalled [seen] | Medium (device port) | **Recommended** |
| **Arch-R** | RGB10 (not Max 2) [inferred] | ❌ no RK3566 device [seen] | Arch/pacman, kernel 6.12, Mesa 26.1 [inferred] | manual AppImage | High (2 device ports) | Plan B if pacman is required |
| **ArkOS** | supported family [inferred] | ✅ (current) | Ubuntu 19.10, kernel 4.19, weston 5 [seen] | ❌ display stack blocks Qt6 [seen] | Low | Only as fallback |
| **Batocera** | ✅ Max/Max 2 [inferred] | likely [inferred] | older stack | limited | Low | Not "bleeding edge" |

**Recommendation:** use **ROCKNIX as the shared core**. RG353V is already a
supported device; the RGB10 Max 2 port is "only" DTB + panel + input + power
quirks, and we already know this board in detail from the Android port (v31
DTS). Keep Arch-R as the alternative if a rolling `pacman` system is a hard
requirement.

### 3.3 Android core

| Device | Base | Why |
|---|---|---|
| RGB10 Max 2 | **andr36oid Android 11** (current), then GSI experiment | no vendor Android 12+ for RK3326; GammaOS does not target it [inferred] |
| RG353V | **GammaOS Lite 12** (LineageOS 19.1 + RK3566 BSP) | stable, rootable, already optimized [seen] |
| RG405V | **GammaOS Next 13/14** (v1.1 for RG405V) | official support for T618; newest Android available [inferred] |

The **andr36oid layer** (L3) is the same on all three: scripts, services,
sysctls, debloat, launcher, ADB/Wi-Fi, Tailscale/Moonlight, input remaps.
Only sysfs paths and package names change per SoC.

### 3.4 What is shared (details in `SHARED-KNOWLEDGE.md`)

- Power/charging quirks, panel fixes, input remaps, mouse mode.
- Performance stack: CPU/GPU/DMC floors, zram, swappiness, readahead, janitor.
- Remote play: Tailscale CLI + Moonlight + Sunshine.
- Android persistence: Magisk `service.d`, wireless ADB, root daemon.
- Linux: ROCKNIX boot chain/quirks, ARMSX2, ES, sway/xdg.
- Build/release: image builder, mtools verification, SD flashing, migration.

---

## 4. Reuse matrix (what carries where)

| Technique | RGB10 Max 2 | RG353V | RG405V |
|---|---|---|---|
| rk817 OTG latch / charging topology | ✅ device-specific | n/a (different PMIC path) | n/a |
| Panel CRTC black-screen cycle | ✅ | not needed (ROCKNIX) | unknown |
| Stick remap / invert / mouse mode | ✅ | port from stock/GammaOS | port (check axes) |
| L3/R3 BTN_THUMBL/R fix | ✅ | verify | verify |
| Performance floor (`gamemode`) | ✅ CPU 1.008 GHz | ✅ CPU/DMC floors | adapt to T618 sysfs |
| zram + swappiness + readahead | ✅ | verify | verify |
| Janitor (`am kill-all`) | ✅ 40 s | ✅ 60 s | adapt interval |
| Wireless ADB persistence | n/a (USB phy off) | ✅ Magisk | target |
| Tailscale CLI daemon | ✅ | target | target |
| Moonlight + Sunshine | ✅ | target | target |
| Image builder / flashing | ✅ mtools/dd | ROCKNIX dd | GammaOS fastboot/Unisoc |
| State migration | ✅ | partial | TBD |
| Linux core / DTS / quirks | port to ROCKNIX | ✅ ROCKNIX native | not planned |

---

## 5. Roadmap

### Phase 0 — Foundation (now)
- ✅ Cross-device master plan + shared knowledge registry + RE method.
- ✅ RGB10 Max 2: Android feature-complete.
- ✅ RG353V: Android optimized, ArkOS/ROCKNIX studied.
- 🔲 RG405V: full recon (when device is available).
- 🔲 Backups: current factory/vendor images of all three devices.

### Phase 1 — RGB10 Max 2 (weakest device, reference implementation)
- Rebuild Android card from `dist/andr36oid-rgb10max2-master.img` + restore
  state (pending new SD).
- Bring up the **shared Linux core** on the Max 2 (ROCKNIX port: DTB, panel,
  input, power/Wi-Fi quirks), or validate an existing RK3326 image first.
- Acceptance: boot Android card and Linux card from the same slot; Wi-Fi,
  charging, input, panel, emulation, streaming all work.

### Phase 2 — RG353V (same core, patched)
- Flash ROCKNIX Generic to **TF1** (Android stays on eMMC, ROMs on TF2).
- Port the andr36oid L3 layer (gamemode/janitor/Tailscale/Moonlight/input).
- Optional: Android 13/14 GSI experiment on a backed-up eMMC (only if a
  rollback path is proven).
- Acceptance: TF1 Linux + eMMC Android + TF2 ROMs; PS2 via ARMSX2; Android
  tweaks at parity.

### Phase 3 — RG405V (powerhouse)
- Full recon (partitions, bootloader, Treble, sysfs, input).
- Install **GammaOS Next** (13/14) with a verified rollback.
- Port andr36oid L3: perf floors, zram, debloat, launcher, ADB/Wi-Fi,
  Tailscale/Moonlight, input remaps.
- Acceptance: stable daily Android, measurably faster than stock, streaming +
  high-end emulation.

### Phase 4 — Community release
- Unified image builder + SD flasher for RGB10 Max 2 and RG353V.
- Versioned releases with checksums, migration scripts, docs.
- Upstream contributions (andr36oid_sdflasher, ROCKNIX quirks/DTB, GammaOS
  feedback).

---

## 6. Documentation protocol

1. **Evidence first.** Every factual claim is tagged **[seen] / [inferred] /
   [unknown]** with a source (command output, file:offset, URL). No exceptions.
2. **Per-device repos** hold device docs; **shared docs** (`MASTER-PLAN.md`,
   `SHARED-KNOWLEDGE.md`, `PROJECT-STATUS.md`, `docs/`) are mirrored into each.
3. **Session ritual:** start from `PROJECT-STATUS.md`; end by updating it + the
   affected device doc + `SHARED-KNOWLEDGE.md` if a reusable finding appeared.
4. **RE backlog** (see `SHARED-KNOWLEDGE.md` §7): anything reverse-engineerable
   is a valid target; record the method and the trap, not just the result.
5. **Work on copies.** Back up before flashing; verify hashes; never trust a
   wiki claim about hardware until observed.

---

## 7. Open decisions

1. **Linux core:** confirm ROCKNIX (recommended) vs Arch-R (pacman/Arch).
2. **RGB10 Max 2 cards:** confirm two separate cards (one Android, one Linux)
   swapped in the single slot.
3. **Android ambition:** how far to push each device (Max 2 GSI 12/13? RG353V
   13/14? RG405V Next 13/14).
4. **RG405V Linux:** investigate only (default) or attempt a T618 port (high
   risk, poor upstream support).
5. **Community release scope/timing:** after Phase 2 or Phase 3?
6. **RG405V access:** when can it be probed (ADB/root state, firmware version)?

---

## 8. Where things live

| Item | Location |
|---|---|
| Cross-device status / restart point | `PROJECT-STATUS.md` |
| Shared techniques/findings | `SHARED-KNOWLEDGE.md` |
| Decisions (ADR log) | `docs/DECISIONS.md` |
| Reverse-engineering method | `docs/RE-METHOD.md` |
| RGB10 Max 2 knowledge base | `rgb10max2-internals/` |
| RG353V knowledge base | `rg353v-internals/` |
| RG405V knowledge base | `rg405v-internals/` |
