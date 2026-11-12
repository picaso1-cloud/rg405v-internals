# Custom GPU drivers on the RG405V (Mali-G52 MP2)

**Last updated:** 2026-11-12

This is the reproducible guide for the MaliSX2/ARMSX2 work, plus what is known
about the same approach in the other emulators. It assumes the RG405V on
GammaOS Next v1.1 Lite (Android 14), Mali-G52 MP2, vendor `mali_gondul` kbase.

---

## 1. The one verified result

**PS2 — ARMSX2 Nightly 20261111 (patched) + MaliSX2 0.4.1 + Renderer Vulkan.**

Capcom vs. SNK 2 booted through the memory-card format to the title/VS-ranking
screen on the RG405V. Confirmed in `emulog.txt`:

```
VKLoader: opening custom Vulkan driver via adrenotools (...) name=libvulkan_malisx2.so
VK: driver is malisx2 (driverInfo "v1.r44p1-malisx2.0.4.1.s9eab19d3cbdb")
VK: GPU 'Mali-G52' vendor=0x13B5 ... fbfetch=yes(in-tile) ...
Vulkan: Swapchain 640x480 ...
GS: Classic renderer active (renderer=Vulkan)
```

---

## 2. The two traps (read this before anything else)

1. **The app hides the driver page on G52.** ARMSX2 stable and nightly compute
   `setMaliSX2Offered(isMaliSX2Supported(gpu))`; `isMaliSX2Supported()` is false
   for Mali-G52, so the MaliSX2 setup step never appears. MaliSX2 **0.4.1**
   itself lists `Mali-G52 / G52 r1 (v7) — experimental`, so the gate is wrong
   for this driver version. The patch forces the flag to true.
2. **`Auto` renderer picks OpenGL on G52.** Even with the driver installed,
   `Auto renderer -> OpenGL, no rule steers this device to Vulkan`. You must
   select **Vulkan** manually; otherwise the custom Vulkan driver is never used.

The same pattern applies to other emulators: *custom driver installed* ≠
*driver used*. Always verify the renderer and the driver name in the log.

**Per-game overrides matter (2026-11-12).** In the last session the GTA San
Andreas profile (`SLUS-20946`) had **no** renderer override, so it inherited the
global `Auto` and ran `renderer=OpenGL` / `GL_RENDERER: Mali-G52` even though
MaliSX2 was installed. CvS2 (`SLUS-20246`) had `renderer=vulkan` and is the one
configured for the custom driver. Check the **per-game** Graphics tab, not just
the global setting. The official stable `com.armsx2` has an empty `drivers/`
directory and `customDriverId=""`; only the patched nightly has MaliSX2
installed.

---

## 3. PS2 — reproduce from scratch

### Files

| File | Purpose | SHA-256 |
|---|---|---|
| `ARMSX2-nightly-20261111-MaliSX2-G52-patched.apk` | patched nightly (debug-signed) | `76924c41…` |
| `malisx2-0.4.1-a833123f35.zip` | MaliSX2 driver | `feb44fcb…` |
| `ARMSX2-nightly-20261111.apk` | official nightly fallback | — |

All are on the SD card at `/storage/00000000-0000-0000-0000-000000000001/Drivers/`
and in `/home/souhail/rg405v-backup/custom-2026-11-12/drivers/`.

### Install

```sh
adb uninstall com.armsx2.nightly          # if the official nightly is installed
adb install ARMSX2-nightly-20261111-MaliSX2-G52-patched.apk
```

### Configure

1. Finish the setup wizard (**App Data Folder**, **BIOS**, **ROMs**).
2. When the **GPU driver** step appears, choose **Download and install**
   (MaliSX2 0.4.1), or **Import** the zip.
3. Start a game, open the pause menu → **Renderer** tab.
4. Select **Vulkan** (not Auto).
5. Under **GPU Driver**, select **malisx2 0.4.1** (it should show **Active**).
6. **Apply & Restart**.

### Verify

`Android/data/com.armsx2.nightly/files/logs/emulog.txt` must contain:

```
VKLoader: opening custom Vulkan driver via adrenotools ... libvulkan_malisx2.so
VK: driver is malisx2
GS: Classic renderer active (renderer=Vulkan)
```

If it says `Auto renderer -> OpenGL` or `GL_RENDERER: Mali-G52`, the renderer
is still OpenGL and the custom driver is not in use.

### Rebuild the patch for a future nightly

```sh
cd rg405v-internals/tools/patches/armsx2-malisx2-g52
./build-patched-apk.sh ARMSX2-nightly-YYYYMMDD-...-Android-arm64.apk
```

This decompiles, replaces the boolean before `setMaliSX2Offered` with
`const/4 v1, 0x1`, reassembles `classes.dex`, swaps it into the APK, zipaligns
and signs with a local debug key. If a nightly changes `ek2.smali`, the script
aborts instead of producing a broken APK; adjust the pattern then.

**Signing:** the patched APK is debug-signed. Official nightly updates require
an uninstall (losing app data) and a re-patch. Keep the official APK and the
stable `com.armsx2` app as fallbacks.

### Test matrix

| Game | Status |
|---|---|
| Capcom vs. SNK 2 | ✅ boots to title/VS ranking on Vulkan+MaliSX2 |
| Metal Slug Anthology | to test |
| Grandia II | to test |
| GTA San Andreas / Vice City Stories | to test; heavy |
| Crash Tag Team Racing | to test; heavy |

G52 is **experimental**. If a game shows black/missing textures or crashes,
switch **GPU Driver → System Vulkan driver** (or OpenGL). MSAA is not
implemented in MaliSX2; games that require it may not start.

---

## 4. Other emulators — what is known

The driver zip format is the same for every app that accepts custom Vulkan
drivers: `meta.json` + `libvulkan_*.so` at the zip root. The **app** decides
whether it exposes an install UI and whether it gates by GPU.

| Emulator | Custom driver support | Driver | Status on this device | Notes |
|---|---|---|---|---|
| **ARMSX2** (PS2) | yes (MaliSX2) | MaliSX2 0.4.1 | ✅ verified | patched nightly + Vulkan |
| **Dolphin** (GC/Wii) | MaliSX2 release notes list Dolphin | MaliSX2 0.4.1 | untested | install zip in Dolphin's GPU driver setting; Vulkan; UI may vary |
| **hakuX** (Xbox) | MaliSX2 release notes list hakuX | MaliSX2 0.4.1 | untested | Vulkan; check driver UI |
| **Vita3K-Plus** (PS Vita) | MaliSX2 release notes: G52 tested by contributor | MaliSX2 0.4.1 | promising | the release notes say Vita3K-Plus has only been played on G52 so far |
| **Eden / Lemon** (Switch) | PanVK is the recommended path | PanVK (Zenithblue / FristOneRR / LukeValen) | untested | T618 CPU is the limit; light games only |
| **Winlator / GameHub** (Windows) | PanVK | Winlator zip / FristOneRR / LukeValen | untested | not a PS2 path; Winlator+PCSX2 is slower than native ARMSX2 |
| **ARMSX3** (PS3) | yes — bundled adrenotools, generic zip loader | MaliSX2 0.4.1 | ❌ closed — memory wall | XMB hangs; direct boot works; gameplay ~20 fps then **OOM kill at 2.7 GB RSS** on 4 GB; see §5 |
| **PPSSPP** (PSP) | no known Mali custom-driver UI | — | use system Vulkan | PPSSPP's own backends |
| **RetroArch** | no per-app Mali driver UI | — | use system Vulkan | a system-wide ICD is the only advanced route |
| **Flycast / Mupen64Plus / DraStic** | no known Mali custom-driver UI | — | use system driver | GLES/Vulkan as provided |

### How to try another emulator

1. Use a build that has a **custom GPU driver** / **GPU driver** setting.
2. Put the driver zip on the SD card (`/Drivers/` is fine).
3. Install the zip from the emulator's driver screen.
4. Select **Vulkan** as the renderer.
5. Start a game and check the app's log for the driver name and `Vulkan`.
6. If the setting is missing, the app may gate by GPU like ARMSX2; check the
   project's source/issues before patching. Do not patch blindly.

### Advanced: system-wide ICD (not recommended yet)

A Mali Vulkan ICD (MaliSX2 or PanVK `.so` + JSON) could be added under
`/system/etc/vulkan/icd.d/` with a Magisk module and forced with
`VK_ICD_FILENAMES`/`wrap.<package>`. This can black-screen Vulkan apps and is
harder to revert than a per-app driver. Use only as an experiment, with the
GammaOS reflash and the verified unbricker as recovery.

---

## 5. PS3 — ARMSX3 (tested 2026-11-12: driver works, XMB hangs, game boot works)

**Short answer: no patch is needed to load MaliSX2 in ARMSX3 1.6.7 — and it
still does not produce a usable PS3.** The blocker is CPU/PPU, not the GPU
driver.

### How the driver was made to load

Unlike ARMSX2, ARMSX3 bundles `adrenotools` inside `libarmsx3-jni.so`
(`adrenotools_open_libvulkan`, `supportsCustomDriverLoading`,
`setCustomDriver`) and its importer honours `libraryName` from `meta.json` —
there is no G52 boolean gate. Two ways to install:

1. Normal: Settings → Renderer → GPU Driver → Import (a zip with `meta.json` +
   `libvulkan_malisx2.so`). The UI was not reachable in Library mode on this
   build, so the driver was sideloaded:
   ```
   /data/data/com.armsx3/files/drivers/malisx2-0.4.1/{meta.json,libvulkan_malisx2.so}
   ```
2. Then point the config at it (app stopped):
   - `shared_prefs/ARMSX2.xml` → `config.global` JSON →
     `"customDriverId":"malisx2-0.4.1"`
   - also add the legacy `<string name="customDriverId">malisx2-0.4.1</string>`

### Result — the driver really works

```
ARMSX3-UI: Loading custom driver .../drivers/malisx2-0.4.1/
ARMSX3-Driver: custom Vulkan driver applied: libvulkan_malisx2.so
MESA: malisx2 build a833123f35f6, driverInfo v1.r44p1-malisx2.0.4.1...
RSX: Vulkan driver identity: 'Mali-G52' ... 'v1.r44p1-malisx2.0.4.1.s9eab19d3cbdb'
RSX: Vulkan: Renderer initialized on device 'Mali-G52'
RSX: Swapchain: 480x640 for a 640x480 window
```

This fixed a hard failure: with the **stock r34 driver** the same XMB boot died
in `vk::render_device::create` (`Verification failed (object: 0x0)`); stock G52
lacks `depthClamp`, shader clip distance, wide lines, depth bounds and
`VK_EXT_shader_uniform_buffer_unsized_array`. MaliSX2 r44p1 supports enough to
create the device and present at ~53 fps.

### Why it still fails

Booting the firmware XMB reached the PPU compile pass and then never converged:

- **`silk_webkit.sprx` was compiled 42 times** between 0:42 and 0:58;
  `sysconf_plugin` 13×, `avc_plugin` 13×, `PEmuCoreLib` 10×.
- The PPU main thread went unresponsive:
  `SIG: Thread [PPU[0x1000000] main_thread] is too sleepy. Waiting for it 9200893us already!`
- At **03:04:59**, after **58 min**, the process exited:
  `reason=2 (SIGNALED) status=9` (SIGKILL), RSS 627 MB.
- 237 `Compiling module` vs 236 `Compiled module` — one compile never finished.

The XMB reload loop points at the XMB's WebKit/browser stack, not at the RSX
driver. The app's overlay (`Progress 31/32`) never moved while the log compiled
dozens of modules, so it is not a usable progress indicator.

### Verdict

ARMSX3's own published minimum is **Snapdragon 865/870, Adreno 650+ or
Mali-G57+, 8 GB RAM**. The RG405V is a **T618 / Mali-G52 MP2 / 4 GB** — below
the floor on every axis. The Minecraft loading screen still renders at ~53 fps,
but that is not a gameplay number; expect low gameplay fps until measured.

- Keep MaliSX2 selected if experimenting; it is strictly better than stock for
  Vulkan device creation.
- The XMB path is a dead end. **Direct game boot works** (`Minecraft`), so use
  that path to measure actual gameplay before judging viability.
- To revert the driver: set `customDriverId` back to `""` (backup of the prefs
  file was kept at `/data/local/tmp/ARMSX2.xml.bak`).

### 2026-11-12 retest — Minecraft (BLUS31426), max clocks — **it boots**

The "cheap game boot" test was run with the strongest settings available:

- `persist.gammaos.performance_mode=max` → both CPU clusters pinned at
  **2.0 GHz** (`userspace`), GPU **850 MHz** (`performance`);
  `fan_mode=max` → pwm 255, `pwm_en=1`, `fan_power=1`.
- MaliSX2 was definitely active:
  `Vulkan driver bound (custom)`, `custom Vulkan driver applied:
  libvulkan_malisx2.so`, `Found Vulkan-compatible GPU: 'Mali-G52' running on
  driver 44.1.0`, identity `v1.r44p1-malisx2.0.4.1`.
- **First correction: it is not stuck.** The first 12 minutes looked like the
  XMB wall (JIT PID churn, no `emulog.txt`), but that was per-module PPU
  compilation while the RSX thread was already rendering. Screenshots show the
  Minecraft loading screen at **~53 fps**, and the progress bar advanced
  **41 % → 48 % → 58 % in 5.5 min** (~3 %/min). Direct game boot bypasses the
  XMB/`silk_webkit` loop that killed the previous test.
- Thermals on `fan_max`: soc **59 → 67 °C**, cputop ~60 °C, GPU ~57 °C; RSS
  0.9–1.2 GB with ~0.4–0.6 GB free. No OOM so far.
- **Gameplay performance is still unmeasured.** The loading screen's 53 fps is
  not a gameplay number; terrain generation is CPU-bound (one A75 at 2 GHz,
  JIT thread 80–100 %). Wait for the world to load before judging playability.
- **XMB remains non-viable; direct game boot is the working path.** This also
  means the earlier "PS3 is not viable" verdict was too broad: the PS3 *boot
  path* works, the *XMB* does not.
- **Final outcome (15:28) — OOM kill in first gameplay.** The compile bar
  completed (module 66/66) ~33 min after launch, the world started, and
  framerate dropped **53 → 23.7 → 19.8 fps** for roughly 90 s of real gameplay.
  RSS then ballooned to **~2.7 GB** on the 4 GB device; `lowmemorykiller`
  breached its watermark with swap also low and killed `com.armsx3` at
  **15:28:44** (`oom_score_adj 0`, foreground TOP app). ARMSX3's own
  next-launch banner confirms: `Previous exit: killed by Android for low
  memory ... (985 MB PSS, 1075 MB RSS)`. A quick relaunch died the same way.
- **Measured gameplay: ~20–24 fps before the OOM.** The limit is **RAM, not
  CPU or GPU**: no clock, fan, or zram setting fixes a 2.7 GB working set on a
  4 GB device (ARMSX3 minimum is 8 GB). **PS3 track closed with measured
  limits:** boots ✓, loads ✓ (~33 min), playable ✗ (OOM in under 2 min).

### Could PS3 ever be playable here? — walls and resume triggers

What we proved in one day: the driver chain works (MaliSX2 on G52 via
adrenotools), direct game boot bypasses XMB, and a real title reaches
**~20 fps of gameplay** on a 2×A75. That is a CPU/JIT result, not a fantasy
number. The remaining walls, and what could clear each:

| Wall | Measured | What would clear it | Plausible? |
|---|---|---|---|
| XMB hang | `silk_webkit` loop | direct boot (done) | **solved** |
| Boot time | ~33 min (first run, PPU LLVM compile) | **PPU cache persists** → second launch should skip most compiles | test now, cheap |
| OOM kill | 2.7 GB RSS vs ~2.3 GB zram + ~2.3 GB RAM | bigger zram (3.5–4 GB zstd), lmkd watermark tuning, in-game render-distance cut | test now, cheap |
| Gameplay fps | ~20 fps at world spawn | upstream JIT/SPU improvements; ARMv9 targets on future chips | wait on upstream |
| Working set | Minecraft PS3 wants ~2.7 GB+ | ARMSX3 upstream memory work (cache eviction, RSX buffers); 8 GB T7xx/T8xx handhelds | hardware or upstream |

**Cheap next tests (unrun):**

1. **Second launch.** The PPU cache was written to
   `cache/BLUS31426/ppu-*/`. If the relaunch loads cached modules, the 33-min
   boot collapses — record the new boot time.
2. **OOM mitigation.** Raise zram to ~3.5–4 GB with zstd (currently 2.3 GB
   lz4), optionally tune `ro.lmk.*` watermarks via `resetprop`, then relaunch.
   Even reaching 10+ min of gameplay at ~20 fps would make light titles
   *borderline playable* rather than impossible.
3. **In-game settings.** If the world opens long enough to open the Minecraft
   menu: cut render distance to minimum — the working set is chunk-driven, so
   this directly attacks the 2.7 GB.

**Resume triggers (ADR-011 style — revisit when ≥2 are true):**

- ~~Second-launch boot time measured under ~10 min with stable RSS~~
  **✅ MET 2026-11-12** — warm cache boots in minutes.
- ~~An OOM-mitigated run sustains ≥15 min gameplay without an LMK kill~~ —
  **partially met**: mitigated runs survive bursts to XMB/title, but no
  in-world time yet.
- ~~A T618-class-or-better handheld with ≥8 GB becomes available~~ — keep.
- ARMSX3 upstream ships a memory-footprint reduction validated by changelog
  (world-gen peak must drop below ~2.3 GB — the measured device ceiling).
- A setting combination reduces the world-gen burst below ~2.3 GB (SPURS/SPU
  thread caps applied in run 7 config are the first candidates).
- MGLRU-capable kernel (≥5.15) becomes available for GammaOS (device is on
  5.4.161 — no MGLRU, no smarter reclaim under pressure).

Until then: PS2 (ARMSX2 nightly + MaliSX2 + per-game Vulkan) is the working
path on this device; PS3 stays documented but parked.

### 2026-11-12 afternoon — staged optimization experiment log

The three "cheap tests" were executed as a staged ladder: one layer per run,
device state verified between runs. **All OS-level tuning is session-only**
(zram/`resetprop`/sysctl revert on reboot). ARMSX3 pref edits persist.

**Layers applied (final state of the session):**

| Layer | Stock | Applied | Mechanism |
|---|---|---|---|
| zram | 2.3 GB lz4 | **4.5 GB zstd** | live: `swapoff` → zram `reset` → `comp_algorithm=zstd` → `disksize` → `mkswap` → `swapon` |
| lmkd swap floor | 10 % | **0 %** | `resetprop ro.lmk.swap_free_low_percentage 0` |
| lmkd PSI | ~70 ms | **150 ms** | `resetprop ro.lmk.psi_partial_stall_ms 150` |
| lmkd thrash limit | 100 | **300** | `resetprop ro.lmk.thrashing_limit 300` |
| watermark scale | 20 | **150** | `/proc/sys/vm/watermark_scale_factor` (300 tried — too greedy, see run 3) |
| ARMSX3 VRAM cap | 2048 MB | **1024 MB** | `ps3VramLimitMb` in `/data/data/com.armsx3/shared_prefs/ARMSX2.xml` (backup: `/data/local/tmp/ARMSX2-r405v.xml.bak`) |
| ARMSX3 SPURS | 6 | **4** | `ps3MaxSpursThreads` (run 7 config, not yet executed) |
| ARMSX3 pref SPU | auto | **4** | `ps3PreferredSpuThreads` (run 7 config, not yet executed) |

**Run log:**

| Run | Config delta | Outcome |
|---|---|---|
| 1 · 14:54 | stock OS | 33-min first boot (PPU LLVM, 66 modules). Loading bar at 53 fps. fps then dropped 53 → ~20–24 (probably title/menu panorama, **not** verified in-world — corrects the earlier "90 s of gameplay" claim). LMK kill 15:28:44 at **2.7 GB RSS**. |
| 2 · ~15:46 | zram 4.5 + lmkd patience | **PPU cache hit — boot minutes, not 33 min.** **XMB rendered and animated** (18–36 fps; `silk_webkit` freeze did not reproduce). World-entry burst → LMK kill 15:59:07 at 2.58 GB; swap only 190 MB used (floor 10 % still active). |
| 3 · ~16:00 | watermark 300 | **Title screen reached** (screenshot proof). kswapd over-swapped: RSS 1.98 GB → 226 MB resident, ~880 MB swapped → 2–3 fps thrash → game unresponsive → `The game did not react to the exit request in time. Terminating manually...` + `VK_ERROR_DEVICE_LOST` (teardown collateral, not a MaliSX2 failure). **Emulator survived and returned to XMB — first app-level survival.** Lesson: watermark 300 swaps out hot JIT code. |
| 4 · 16:09 | watermark 150 | Healthy boot 51 fps; world-create menus 20–27 fps; **lmkd killed a 3 MB background process instead of the game** — staged protections working as designed. World-gen burst → fps 13→9→3 → **RSX thread fatal: `Verification failed (object: 0x0)`** right after video-memory heap growth (`heap 640M of 2048M cap, driver reports 647M of 1866M budget`). Memory was fine at death (959 MB free) — first *emulator-internal* failure. |
| 5 · 16:16 | — | World created (default menus). **Kernel global OOM**: RSS peak **2919 MB**, `System: 3538/3538MB used (0MB free)`; `dmesg: oom-kill … global_oom, task=com.armsx3`; whole app killed. |
| 6 · 16:19 | VRAM 1024 | Boot visibly lighter (RSS 1.6 GB at ESRB vs 2.6 in run 5). World-gen burst identical: 255 MB free → **0 MB in 10 s**, RSS peak **2816 MB**, global OOM again. **Conclusion: the burst is game-data driven (guest RAM + LLVM caches), not graphics-heap driven — VRAM cap is not the lever.** |

**User-verified constraint:** Minecraft PS3 (Legacy Console) has **no world-size
option** — PS3 worlds are fixed at the smallest legacy size (864×864). The
measured gen burst is therefore already the game's minimum.

**Measured kill chain (hierarchical, all observed):**
`lmkd per-app kill` (runs 1–2) → `watermark breach` (runs 1–2) →
`kernel global OOM` (runs 5–6, final and uncatchable). The staged OS tuning
moved the failure point from "app killed with 2.3 GB zram lz4" to "app
survives bursts up to ~2.8 GB and returns to XMB" — real progress, but the
last 100 MB is the wall: **ARMSX3 2.9.3 world-gen needs a ~2.9 GB peak burst;
the device OOMs at ~2.8.**

**What is proven to work now:**
- Boot in minutes via warm PPU cache (vs 33 min cold).
- XMB fully interactive; title screen; world-create UI; on-screen keyboard —
  all rendered, up to 50 fps with the final config.
- App-level survival through memory bursts (no LMK kill in runs 3–4).

**Run 7 (pending):** SPURS 4 + preferred SPU 4 + VRAM 1024. If world-gen still
OOMs identically, the floor is confirmed for this ARMSX3 build and the track
parks until upstream memory work or 8 GB-class hardware.

**Revert paths:** OS tuning dies on reboot; prefs: restore
`/data/local/tmp/ARMSX2-r405v.xml.bak` over `ARMSX2.xml` (app stopped).

Revert with `setprop persist.gammaos.performance_mode stock` and
`setprop persist.gammaos.fan_mode auto` when the test is over.

## 6. Recovery and fallbacks

- **System driver:** ARMSX2 → pause → Renderer → GPU Driver → System Vulkan
  driver (or OpenGL). The stock driver is untouched.
- **Official app:** keep the official nightly and stable APK; uninstall the
  patched app if needed.
- **ROMs/saves:** ROMs on the SD card are untouched. App data (memcards, saves)
  is in the app's data folder; back it up if you uninstall.
- **System:** GammaOS reflash = soft-brick recovery; official RG405V unbricker =
  hard-brick/stock recovery. See `RECOVERY.md`.

---

## 7. Maintenance checklist

- [ ] Every new ARMSX2 nightly: re-run `build-patched-apk.sh`, test Vulkan +
      MaliSX2, keep the previous working APK.
- [ ] Every new MaliSX2 release: update the zip, clear shader cache, re-test
      CvS2 / Metal Slug / Grandia II; record results.
- [ ] Test one Dolphin/hakuX/Vita3K-Plus game with the same zip; record whether
      the app exposes the driver UI and whether G52 is stable.
- [ ] Keep the official APKs, driver zips and `MALISX2-WORKING.txt` on the SD
      card and in the customisation backup.
- [ ] If G52 artifacts appear: switch to system driver, capture logcat/emulog,
      report to the driver project (MaliSX2 or PanVK).
