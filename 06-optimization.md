# 06 — Performance tuning (RG405V, stock Android 12, no root)

**Date:** 2026-11-08 · **Access:** USB ADB · **Scope:** user-space, reversible.
No root, no bootloader changes, no system partitions touched.

## Applied

### 1. Daijishō as the default launcher

```sh
cmd package set-home-activity com.magneticchen.daijishou/.activities.MainActivity
```

- Resolver now reports `com.magneticchen.daijishou/.activities.BootstrapActivity`
  as the default HOME.
- Pressing **Home** lands in Daijishō `MainActivity` (verified with
  `dumpsys window`).
- Revert: `cmd package set-home-activity com.android.launcher3/.uioverrides.QuickstepLauncher`

### 2. Animation scales → 0.5×

```sh
settings put global window_animation_scale 0.5
settings put global transition_animation_scale 0.5
settings put global animator_duration_scale 0.5
```

Verified: all three read back `0.5`. Revert with `1.0`.

### 3. Battery-optimization exemptions (prevents Doze freezing)

Whitelisted with `cmd deviceidle whitelist +<package>`:

- `com.magneticchen.daijishou`
- `xyz.aethersx2.android`, `org.dolphinemu.dolphinemu`, `org.dolphinemu.mmjr`,
  `org.ppsspp.ppsspp`, `com.github.stenzek.duckstation`,
  `com.retroarch.aarch64`, `org.citra.emu`, `com.flycast.emulator`,
  `org.mupen64plusae.v3.fzurita.pro`, `com.dsemu.drastic`,
  `io.recompiled.redream`, `org.devmiyax.yabasanshioro2.pro`,
  `org.openbor.engine`, `org.easyrpg.player`
- `com.looker.droidify`, `com.limelight`

This does **not** grant background network/CPU; it only stops Android from
killing these apps when the screen sleeps.

### 4. AOT compilation (`speed`)

```sh
cmd package compile -m speed -f <package>
```

All returned **Success** for: Daijishō, AetherSX2, Dolphin, Dolphin MMJR,
PPSSPP, DuckStation, RetroArch, Citra, Flycast, Mupen64Plus AE, DraStic,
Redream, Yabasanshiro, OpenBOR, EasyRPG, Droidify, Moonlight.

This precompiles DEX → native ahead-of-time, improving cold start and reducing
JIT overhead. Revert a package with `cmd package compile --reset <pkg>` or
`cmd package compile -m verify -f <pkg>`.

### 5. Safe, reversible Google debloat

Disabled with `pm disable-user --user 0`:

- `com.google.android.apps.photos`
- `com.google.android.apps.restore`
- `com.google.android.feedback`
- `com.google.android.onetimeinitializer`
- `com.google.android.partnersetup`
- `com.google.android.printservice.recommendation`

**Not touched:** GMS, Play Store, Chrome, Gboard, WebView, network stack,
permission controller, TTS, Files — these are required for normal operation.
Re-enable any with `pm enable <package>` (or `pm enable --user 0`).
`com.android.nfc` and `com.sprd.powersavemodelauncher` were already disabled
before this session; not changed by us.

## Not applied (and why)

- **CPU/GPU governor locking:** `/sys/.../scaling_governor` is root-only; shell
  cannot write it. A root/Shizuku ADB-level tool cannot either.
- **Fixed-performance mode:** `cmd power set-fixed-performance-mode-enabled`
  exists on some builds but would pin max clocks → heat/battery. Left off.
- **Disabling GMS/Play:** breaks Play/Chrome-dependent behavior; not worth it
  on a stock device. Lite/GammaOS Next is the proper path for a no-GMS build.
- **Chrome background restriction:** left default (browser still useful).
- **Aptoide:** third-party store still installed; may run in background. Could
  be disabled if unused (Droidify replaces it).

## Verification snapshot

- HOME = `com.magneticchen.daijishou/.activities.BootstrapActivity`
- Home focus = `com.magneticchen.daijishou/...MainActivity`
- animations = `0.5 / 0.5 / 0.5`
- 17 packages AOT-compiled, all `Success`
- 17 packages battery-whitelisted
- 6 Google packages disabled-user

## Next

- Add Daijishō ROM paths (`/storage/EAFBBC4089760D1F/<platform>`) and pick
  emulators per platform — the launcher is default but the library is empty
  until paths are set.
- Optionally install Cromite/Fennec from Droidify as the browser.
- Update PPSSPP/RetroArch/Flycast/Azahar from the prepared APKs.
