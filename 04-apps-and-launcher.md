# 04 — Installed apps, launcher & rootless tools (RG405V)

**Date:** 2026-11-08 · Device online via wireless ADB `192.168.1.189:43815`.
All installs below are user-space APKs; **no root, no bootloader unlock, no
system partition changes**.

## Installed (verified)

| App | Version / package | Source | Status |
|---|---|---|---|
| **Daijishō** | 1.5.0 · `com.magneticchen.daijishou` | [GitHub v1.5.0](https://github.com/TapiocaFox/Daijishou/releases/tag/v1.5.0) | ✅ installed; **32 platforms imported**; ROM paths still to add |
| **Droidify** | 0.7.8 · `com.looker.droidify` | [GitHub v0.7.8](https://github.com/Droid-ify/client/releases/tag/v0.7.8) | ✅ installed — F-Droid client for FOSS apps |
| **Shizuku-Next** | 14.0.33-next · `moe.shizuku.privileged.api` | [GitHub v14.0.33](https://github.com/rushiranpise/Shizuku-Next/releases/tag/v14.0.33-next) | ✅ installed + **running** (`shizuku_server`, UID 2000/shell, transport `adb`) |

### Shizuku-Next notes

- Started via its wireless-debugging pairing flow (`adb pair` from the PC +
  `Other start methods` in the app). UI now shows **Running · Wireless · adb ·
  UID 2000 (shell)**.
- After a reboot the service stops: open Shizuku-Next and press **Start**, or
  enable **start on boot** in its settings (needs battery-optimization
  exemption; the onboarding already set it to Allowed).
- It grants ADB-level (shell UID) permissions to apps — useful for rootless
  tools, but **not** kernel/CPU governor writes (those still need real root).

## Prepared but not installed (on the PC)

| App | Version | Note |
|---|---|---|
| Cromite | 153.0.8010.37 arm64 · `org.cromite.cromite` | open-source Chromium browser; installs alongside Chrome |
| Fennec | 159.0.0 arm64 · `org.mozilla.fennec_fdroid` | F-Droid Firefox (download was interrupted; can fetch) |
| PPSSPP | 1.20.4 | update from preinstalled 1.11.3 |
| RetroArch | 1.22.2 aarch64 | update from preinstalled 1.10.3_GIT |
| Flycast | 2.7 | update from preinstalled V2.1 |
| Azahar | 2126.2 | new 3DS emulator (Citra successor) |

> The preinstalled PPSSPP/RetroArch/Flycast live in `/system/preloadapp` with
> stripped signatures; installing official APKs may require uninstalling the
> old copy first (saves/configs are on the SD card and backed up).

## Chrome

- Device has **no Google account**; Chrome's only official update path is Play
  Store. Options: sign in and update; install **Aurora Store** for anonymous
  Play updates; or use **Cromite** as the daily browser.
- Chrome was **not** modified.

## Next steps

- [ ] Daijishō: add ROM paths (`/storage/EAFBBC4089760D1F/<platform>`), pick
      emulators, set as default home.
- [ ] Install FOSS apps/browsers from **Droidify** (official F-Droid repo).
- [ ] Apply safe optimizations: animation scales 0.5, battery exemption for
      Daijishō/emulators, AOT compile, optional reversible Google debloat.
- [ ] Update emulators (PPSSPP/RetroArch/Flycast/Azahar) after Daijishō is set.
