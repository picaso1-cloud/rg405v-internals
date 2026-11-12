# ARMSX2 MaliSX2 on Mali-G52 — patch + build

**Confirmed working 2026-11-12** on the RG405V (T618, Mali-G52 MP2, 4 GB):
Capcom vs. SNK 2 booted to the title/character-select screen on
Vulkan + MaliSX2 0.4.1.

## Why a patch is needed

ARMSX2 gates the MaliSX2 setup page behind a native flag,

```
NativeApp.setMaliSX2Offered(isMaliSX2Supported(gpu))
```

and `isMaliSX2Supported()` is false for Mali-G52 in the stable and nightly
builds. MaliSX2 0.4.1 itself **does** list `Mali-G52 / G52 r1 (v7)` as
experimental. Forcing the flag to true reveals the setup step and the GPU
driver list; the driver then installs and loads normally.

Second trap: the renderer must be **Vulkan**. On this GPU ARMSX2's `Auto`
renderer picks **OpenGL** (`Auto renderer -> OpenGL, no rule steers this
device to Vulkan`), so the custom Vulkan driver is not used until you select
Vulkan manually.

## Files

- `patch-ek2.py` — replaces the boolean computation before
  `setMaliSX2Offered` with `const/4 v1, 0x1`.
- `build-patched-apk.sh` — decompiles the nightly, applies the patch,
  reassembles `classes.dex`, swaps it into the original APK, zipaligns and
  signs with a local debug key.

## Build

```sh
# dependencies: apktool (2.12+), apksigner, zipalign, keytool, java
./build-patched-apk.sh ARMSX2-nightly-20261111-...-Android-arm64.apk
```

Output: `build/ARMSX2-MaliSX2-G52-patched.apk` (debug-signed).

Install as a separate app / update:

```sh
adb uninstall com.armsx2.nightly        # if an older nightly is installed
adb install build/ARMSX2-MaliSX2-G52-patched.apk
```

The patched app has a different signature from the official nightly, so future
official updates require uninstalling it first (app data is lost; the driver
zip and BIOS/ROMs are external).

## Run

1. Setup → **GPU driver** → **Download and install** `malisx2 0.4.1`
   (or **Import** the zip).
2. In-game pause → **Renderer** tab → **Vulkan**.
3. **GPU Driver** → `malisx2 0.4.1` → **Active**.
4. **Apply & Restart**.

Proof in `Android/data/com.armsx2.nightly/files/logs/emulog.txt`:

```
VKLoader: opening custom Vulkan driver via adrenotools ... libvulkan_malisx2.so
VK: driver is malisx2 (driverInfo "v1.r44p1-malisx2.0.4.1...")
GS: Classic renderer active (renderer=Vulkan)
```

## Caveats

- G52 is **experimental**. If a game shows artifacts, black textures or
  crashes, switch back to the System Vulkan driver.
- The patch targets the current nightly's `ek2.smali` block. If a new nightly
  changes the code, adjust the pattern; the script checks and aborts if the
  block is not found.
- Keep the official nightly and stable APK as fallbacks.
