# 09 — Mods: mouse mode, USB modes, HID gamepad, control panel, tuning

**Date:** 2026-11-11 · **Device:** RG405V (T618) · **OS:** GammaOS Next v1.1 Lite
(Android 14) · **Root:** Magisk 26.1 / app 29.0

Everything below was built and tested on the device. Sources live in
[`tools/`](tools/) of this repo.

Evidence tags: **[seen]** measured here · **[inferred]** · **[unknown]**.

---

## 1. Mouse mode (gamepad → pointer)

GammaOS ships a `GammaPad Virtual Mouse` device but exposes **no tile or chord**
for it, so this is our own daemon.

| Item | Value |
|---|---|
| Binary | `/data/local/tmp/mousemode` (static aarch64, ~870 KB) |
| Source | [`tools/mousemode/mousemode.c`](tools/mousemode/mousemode.c) |
| Toggle | **hold L1 + R1 + Select** |
| Pointer | right stick (`ABS_Z`/`ABS_RZ`) |
| Scroll | left stick (`ABS_Y` vertical, `ABS_X` horizontal) |
| Clicks | **A = left, B = right, X = middle** |
| Capture | the pad is `EVIOCGRAB`'d while active, so games stop receiving pad input |

The daemon auto-detects the pad by **capabilities** (not just by name) — it
ignores the axis-less `GammaPad Virtual Mouse` and picks `event7`
(`Xbox Wireless Controller`, score 10).

### 1.1 Configuration

`/data/local/tmp/mousemode.conf` (read at startup):

```ini
speed=700        # px/second at full deflection (default 700; was 1400 = too fast)
deadzone=300     # of 1800
invert_y=0
ab_left=1        # 1 = the button labelled "A" produces a LEFT click
```

`ab_left=1` exists because this unit uses the **Nintendo face layout**: the
button labelled **A** is the *east* face button, so it emits `BTN_EAST`. Without
the swap, A behaved as a **right** click. Verified on the device.

### 1.2 Autostart

`/data/adb/service.d/50-mousemode.sh` (Magisk late-start, root) starts the
daemon 25 s after boot if it is not already running.

---

## 2. andr36oid Panel (settings app)

Path: [`tools/controlpanel`](tools/controlpanel) · APK ~17 KB · no Gradle.

| Section | Controls |
|---|---|
| Mouse mode | on/off switch, speed slider (200–3000), deadzone slider, invert Y, **A = left click**, Apply & restart |
| USB mode | MTP / PTP / RNDIS / MIDI / ADB / Charging |
| Performance | Stock / Max / Powersave (`persist.gammaos.performance_mode`) |
| USB gamepad (HID) | Enable (with warning) / Restore USB |
| Status | mouse ON/off · profile · USB function · HID off/ACTIVE |

Implementation notes:
- Programmatic UI, no resources; runs every action through Magisk `su`.
- Built with `aapt2` + `javac` + `d8` + `apksigner` against
  `/opt/android-sdk/platforms/android-34/android.jar`; see `build.sh`.
- Install: `adb install -r out/panel.apk`, then launch
  `org.andr36oid.panel/.MainActivity`.
- Settings persist in `SharedPreferences` and are re-written to
  `mousemode.conf` on every apply.

---

## 3. USB modes

Vendor init supports (confirmed by grepping `/vendor/etc/init`):
`mtp`, `ptp`, `rndis`, `midi`, `mass`, `cdrom`, `accessory`, `vser`, `gser`,
`audio`, `none`, and `adb` combinations.

```sh
sh /data/local/tmp/usbmode.sh mtp|ptp|rndis|midi|adb|charging|status
```

The same is available in the panel. `svc usb setFunctions` is the transport.

---

## 4. USB HID gamepad (device → PC)

### 4.1 Why it needs a script

The kernel has `CONFIG_USB_CONFIGFS_F_HID=y` and the UDC is
`musb-hdrc.1.auto`, but Android's USB HAL only instantiates
`ffs.adb / gser / vser / midi / mass_storage`. Binding a second gadget fails
with **`EBUSY`** because Android re-binds `g1` immediately.

Working sequence ([`tools/hidpad/hid-gadget.sh`](tools/hidpad/hid-gadget.sh)):

1. `stop usb-hal-1-1` (the HAL otherwise re-binds within ~1 s)
2. `setprop sys.usb.config none` → unbind `g1`
3. create `/config/usb_gadget/g2` with `hid.usb0` (42-byte report descriptor,
   6-byte reports: X/Y/Z/Rz + 16 buttons)
4. `echo musb-hdrc.1.auto > g2/UDC`
5. run `hidpad` (reads the pad, writes `/dev/hidg0`)

Restore: unbind `g2`, delete it, `start usb-hal-1-1`,
`setprop sys.usb.config adb`.

### 4.2 Self-restore watchdog

`hid-gadget.sh start <seconds>` schedules `stop` after N seconds. This is
**important**: switching to HID disables USB ADB *and* restarts adbd, which also
drops the TCP (`5555`) listener. With the watchdog the device always comes back
without a reboot. The panel's "Restore USB" does the same on demand.

### 4.3 Host-side result [seen]

```
1209:0002  andr36oid / RG405V HID Gamepad
hid-generic ... USB HID v1.01 Gamepad
/dev/input/js0, event18, hidraw2
```

- Buttons tested: A, B, X, Y, L1, R1 — 30+ events each.
- Axes tested: all four, full range ±32767, 926 events in one capture.
- D-pad maps to buttons 12–15.

---

## 5. D-pad in Android UI — native works (bridge retired)

**Final state [seen]:** the controller D-pad navigates Android Settings **natively**
— one row per press, **A opens**, **B goes back**. No helper is needed.

History / why the bridge exists:

- Early on the D-pad appeared to move only between groups. A Tab bridge
  (`tools/dpadkeys`) was written because a native `input keyevent 20` only
  focused `homepage_container` while rows were reachable with Tab.
- After the **touch controller was reset with a full power-off** (see §8), the
  native D-pad behaviour started working row-by-row, including A/B.
- With the bridge running, every press moved **two** rows: the pad already moved
  one row and the injected Tab moved another. Logs showed exactly one Tab per
  press, which is how the double source was identified.

The bridge is therefore **disabled**: daemon stopped and
`/data/adb/service.d/52-dpadkeys.sh` removed. The source stays in
`tools/dpadkeys/` for builds where the native mapping regresses; the Panel's
"D-pad bridge" switch defaults to **off**.

---

## 6. Tuning applied

| Change | Value | Reversible |
|---|---|---|
| Performance profile | `stock` (schedutil 614 MHz–2.0 GHz, GPU simple_ondemand) | `perfmode.sh stock` |
| Animations | window/transition/animator `0.5` | settings put … 1.0 |
| AOT compile (`speed`) | Daijishō, RetroArch, PPSSPP, Flycast, DraStic, Mupen64, Firefox, Aurora, Mixplorer, Magisk | — |
| Disabled packages | `printspooler`, `bookmarkprovider`, `egg`, `dreams.basic`, `wallpaper.livepicker` | `pm enable <pkg>` |
| ADB over Wi-Fi | `service.adb.tcp.port 5555` via `51-adb-wifi.sh` | delete the script |

Not copied from the RGB10 (1 GB) recipe: aggressive zram/swappiness and
app-killing. This device has 4 GB and `swappiness=150` already; compressing more
would only burn CPU. Measure before changing.

---

## 7. microSD card: full validation

The card previously produced corrupt files on the phone. Full destructive test
on the laptop card reader:

| Test | Result |
|---|---|
| Capacity | 117.2 GB reported, 118 GB exFAT |
| `f3write` (fill 118 GB) | **118/118 files OK**, avg **47.3 MB/s** |
| `f3read` (verify all) | **Data OK 117.18 GB, LOST 0, Corrupted 0**, avg **84.2 MB/s** |

**Verdict: the card is healthy.** The earlier corruption was most likely the
phone's SD interface or a stale FTL state, not failing NAND.

One caveat: during the first copy of the ROM set, **one file differed by a single
bit** (`Tomb Raider - Legend`, byte 888,225,265, `0x86` → `0x8E`). Recopied and
re-hashed: identical. A second full pass over all 32 files: **0 mismatches**.
Keep checksums on future bulk copies.

### Test set copied (5.75 GiB, 26 ROMs + BIOS)

| System | Files | System | Files |
|---|---:|---|---:|
| neogeo | 6 | psp | 3 |
| snes | 5 | ps2 | 2 |
| nds | 4 | openbor | 3 |
| psx | 3 | bios | 6 |

Layout on the card: `ROMs/<system>/…`, `ROMs/bios/` (scph5500/5501/5502,
scph7001, psxonpsp660, neogeo.zip). Manifest: `ROMs/_TEST-SET.tsv`.


---

## 8. Touchscreen: stuck controller and the cold-boot fix

During the USB/HID experiments the Goodix panel stopped reporting touches.

What the evidence showed:

- `/proc/interrupts` for `gt9xx` **incremented while tapping** (72 → 196).
- `dmesg` logged fresh coordinates (`goodix-ts ... [0][x y n]`).
- `getevent` / `cat /dev/input/event0` returned **0 bytes**.
- Driver unbind/rebind re-initialised cleanly (`Input report thread enabled!`)
  but still did not report.
- A normal **reboot did not fix it**; a **full power-off (`reboot -p`) + power
  button** did.

Also learned: while the screen sleeps, Android grabs the touch node for wake
gestures, so `getevent` shows nothing and taps look dead. For remote testing use
`svc power stayon true` and wake the screen first (`input keyevent 224`).

If it happens again: power off completely (do not just reboot), then boot.

---

## 9. Wi-Fi: the Settings dialog dismisses on Connect (this is normal)

**Seen 2026-11-12** on GammaOS Next v1.1 (TrebleDroid GSI, Android 14,
`lineage_arm64_bvN-userdebug 14 AP2A.240905.003`).

Symptom reported: in **Settings → Network & internet → Wi-Fi**, enter the
passphrase, tap Connect, and the dialog closes and returns to the Wi-Fi list.
It looks like the join failed.

What the logs actually show:

```
WifiService: connect uid=1000 packageNameToUse=com.android.settings
WifiConfigManager: Cannot find network with networkId -1 ... (normal for a new network)
WifiConfigManager: Credential changed for netId=2
wpa_supplicant: Trying to associate with SSID 'Tenda_ED9470'
wpa_supplicant: CTRL-EVENT-CONNECTED ... completed
ConnectivityService: ... WIFI CONNECTED ... IS_VALIDATED
```

The network was saved (`netId 2`), associated, got `192.168.1.192`, and passed
internet validation. The dialog **submits and finishes immediately by design**;
the association runs in the background. The Wi-Fi list shows
`Connecting… → Obtaining IP address… → Connected` within ~2 s.

Cosmetic bug to ignore: when the dialog is opened from the **status
bar/Quick Settings**, logcat shows

```
WifiDialogActivity: Failed to get the calling package, don't return the result.
```

The dialog was launched by `com.android.systemui` (uid 10140) without a calling
package, so it skips its result callback. The connect request is still issued.
Both entry points work; the Settings app one simply returns to the list.

Practical notes:

- After tapping Connect, wait 2–3 s and watch the list; it should go to
  **Connected**. If it stays on **Saved** or shows **Authentication problem**,
  the passphrase or security type is wrong.
- Check state from adb without the UI:
  `cmd wifi status`, `cmd wifi list-networks`.
- Do **not** "Reset Wi-Fi, mobile & Bluetooth" as a first step; it only deletes
  the saved networks.
- The same dialog result issue does **not** affect the saved-network reconnects.

Separate observation (not Wi-Fi): this GSI reports **640x480** from `wm size`,
not the RG405V panel's 960x1280. That is a display-config/overlay issue in the
GSI, worth investigating separately from the network stack.

---

## 10. Logging hygiene: the sprd-wlan kernel flood

**2026-11-12.** The Unisoc Wi-Fi driver (`sprd-wlan`, kernel `kworker` threads)
logs every management frame at `I`/`W` level into the **kernel** log buffer.
During association it emitted ~30k lines in about a minute, rotating away the
application logs and making PS2/emulator debugging impossible.

Facts:

- The tag is kernel-side, so `log.tag.sprd-wlan` does **not** silence it.
- Default logd buffers are only **256 KiB** each.
- `sprd_wlan` exposes no debug-level sysfs parameter; `sc2355_sdio_wlan` only
  has `wfa_cap`.
- One real driver error is visible in the flood:
  `sc2355_rx_rsp_process ... CMD_RND_MAC ... SPRD_CMD_STATUS_EXEC_ERROR`
  (the MAC-randomisation command fails; the radio keeps using the factory MAC).

Applied:

- logd buffers resized to **4 MiB** each (`logcat -b main/system/crash/kernel
  -G 4M`) and persisted via `persist.logd.size* = 4M`.
- `/data/adb/service.d/60-log-tuning.sh` re-applies this at boot.
- `tools/rg405v-log.sh` splits the streams on the host:
  - `app` → `main+system+crash` (no sprd-wlan)
  - `wifi` → `kernel` filtered to `sprd-wlan`
  - `start`/`stop` → separate `logs/rg405v/app-*.log` and `wifi-*.log`
  - `snapshot` → timestamped files

Use it instead of `logcat -b all`:

```sh
tools/rg405v-log.sh app 2000
tools/rg405v-log.sh wifi 300
tools/rg405v-log.sh start      # ... tools/rg405v-log.sh stop
```

---

## 11. Display: 640x480 is the RG405V's native panel

**Corrected 2026-11-12.** Anbernic's official RG405V spec is a **4-inch IPS
touch screen, 640x480**, with **Mali-G52 MP2**. `wm size` = 640x480 is therefore
**correct**, not a bug.

The kernel cmdline is confusing but consistent with that:

```
lcd_id=IDffff
lcd_name=lcd_d53e6ea8966_mipi_2lane_960X544
lcd_size=640x480
```

`lcd_name` is a shared panel-driver/timing name; the active `lcd_size` is
640x480 and the DRM mode is 480x640 portrait (`ORIENTATION_90`), which is
640x480 in landscape. The RG505 theory was wrong. `wm size`/`wm density` should
be left alone.

The RG405V GammaOS Next v1.1 Lite package was already downloaded and verified
locally (`/home/souhail/rg405v-gammaos/`, 5 parts, 2.42 GB) and is the correct
RG405V variant; the flash log shows the standard `FlashPartitions.sh` flow.
No re-download/reflash is required for the screen.

The **RG405V unbricker** was downloaded and verified on 2026-11-12:
`RG405V_UNBRICKER_SPD_Upgrade_Tool_R26.21.2801.7z`, 2,549,946,021 bytes,
SHA-256 `9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71`; `7z t` OK (115 files). The rollback gate is now covered.

---

## 12. PS2 session findings — ARMSX2 2.9.3 (2026-11-12)

Session log: `/sdcard/Android/data/com.armsx2/files/logs/session.log`.

- GPU is **Mali-G52 MP2** (`GLES: ARM, Mali-G52, OpenGL ES 3.2 v1.r34p0...`),
  **not G57**. MaliSX2 does not cover G52, so ARMSX2 is on the stock driver;
  no custom driver is available for this device.
- Memcards are valid: 8,650,752 bytes, header
  `Sony PS2 Memory Card Format 1.2.0.0`, mtime at session end. The log's
  `MCDBAK ... UNREADABLE` is the app's **auto-backup** step failing, not the
  memcard itself.
- Auto-tune is on. Committed per-game values:
  - GTA Vice City Stories: `fpsLimit:30`
  - Crash Tag Team Racing (SLUS-21191): `hwScaler:1`,
    `hardwareDownloadMode:5`
- Playtime: SLUS-21191 (Crash) 1002 s. Session CPU: 41m40s user + 2m15s
  kernel, 45m49s foreground.
- The log shows `ANDROID_SWAP_DISC` to other games (Crash, CvS2, GTA SA, NFS
  Underground) while GTA VCS was running. Use the game list to switch titles;
  disc swap is for multi-disc games.
- OSD FPS was off, so no frame-rate numbers were recorded. Enable it in
  ARMSX2 → Settings → OSD for the next run.

Expectation: T618 + Mali-G52 MP2 is below the comfortable PS2 floor. 2D and
light 3D titles (CvS2, Metal Slug Anthology, Grandia II) are the targets;
GTA/Crash will be ~15–30 fps even at 1x native with auto-tune.

---

## 13. Customisation backup (2026-11-12)

Full local backup of the device customisations:

`/home/souhail/rg405v-backup/custom-2026-11-12/` — 14 MB, 127 files,
`MANIFEST.sha256`, `README.md` with restore commands.

Contents:

| Path | Contents |
|---|---|
| `service.d/` | `50-mousemode.sh`, `51-adb-wifi.sh`, `60-log-tuning.sh` |
| `local-tmp/` | `mousemode` + conf, `hidpad` + HID descriptor + `hid-gadget.sh`, `dpadkeys` + conf (**retired**), `usbmode.sh`, `perfmode.sh`, logs |
| `apks/` | `org.andr36oid.panel.apk` |
| `appdata/` | Panel + Daijishō raw data (reference/selective restore) |
| `settings/` | global/system/secure, packages, props, overlays, USB mode, Magisk, performance mode |

Repo copies of the scripts: `tools/device-scripts/`.

Restore notes: do **not** re-enable `52-dpadkeys.sh` — the D-pad is native.
Settings dumps are reference; do not blindly restore `settings-secure.txt`
(device identifiers). The Panel APK and Daijishō can be reinstalled/re-pointed
more safely than raw app-data copy.

State at backup: GammaOS Next v1.1 Lite (Android 14, `AP2A.240905.003`),
Magisk 26.1/app 29.0, `performance_mode=stock`, USB `adb`, log buffers 4 MiB.

---

## 14. MaliSX2 on Mali-G52 — WORKING (2026-11-12)

**Confirmed on hardware:** patched ARMSX2 Nightly 20261111 + **MaliSX2 0.4.1**
+ **Renderer = Vulkan** booted **Capcom vs. SNK 2** through the memory-card
format to the title/character-select screen on the RG405V (T618, Mali-G52 MP2,
4 GB).

Emulator log proof:

```
VKLoader: opening custom Vulkan driver via adrenotools (...) name=libvulkan_malisx2.so
VK: driver is malisx2 (driverInfo "v1.r44p1-malisx2.0.4.1.s9eab19d3cbdb")
VK: GPU 'Mali-G52' vendor=0x13B5 ... | ROAA=yes fbfetch=yes(in-tile) ...
Vulkan: Swapchain 640x480 ...
GS: Classic renderer active (renderer=Vulkan)
```

### Why the app hides it

ARMSX2 gates the MaliSX2 setup step behind

```
NativeApp.setMaliSX2Offered(isMaliSX2Supported(gpu))
```

and `isMaliSX2Supported()` is false for Mali-G52 in the stable 2.9.3 and the
20261111 nightly. The driver itself **does** support G52: MaliSX2 0.4.1 lists
`Mali-G52 / G52 r1 (v7) — experimental`, and its notes report God of War
Collection at ~30 fps on an RG Vita (T618/G52 MP2).

### Required setup

1. **Patched nightly APK** — force `setMaliSX2Offered(true)`; the build script
   and patch are in `tools/patches/armsx2-malisx2-g52/`.
   Ready-made: `/storage/.../Drivers/ARMSX2-nightly-20261111-MaliSX2-G52-patched.apk`
   (SHA-256 `76924c41…`; debug-signed — uninstall the official nightly first).
2. **MaliSX2 0.4.1** — `malisx2-0.4.1-a833123f35.zip` (on the card).
3. Setup → **GPU driver** → **Download and install** (or Import).
4. **Renderer = Vulkan.** This is the second trap: `Auto` picks **OpenGL** on
   G52 (`Auto renderer -> OpenGL, no rule steers this device to Vulkan`), so
   the custom Vulkan driver is never used.
5. GPU Driver → `malisx2 0.4.1` (Active) → **Apply & Restart**.

### Caveats

- G52 is experimental. If a game shows black/missing textures or crashes,
  switch to **System Vulkan driver** (or OpenGL).
- The patched APK is debug-signed; an official nightly update needs an uninstall
  (data loss) and a re-patch. Keep the official APK and the build script.
- The patch only changes the offered flag; the driver loads through ARMSX2's
  normal adrenotools path.
- PanVK is no longer needed for PS2; keep it for Winlator/GameHub experiments.

---

## 15. Next steps

1. Insert the card into the phone; confirm `public:…` mounts and
   `/storage/<uuid>/ROMs` is readable.
2. Point Daijishō at the seven platforms; run one title per system.
3. Decide whether `51-adb-wifi.sh` should stay enabled (LAN exposure).
4. Commit the panel/tools to the repo (done) and keep the APK in `tools/controlpanel/out`.
5. Unbricker still pending — rollback unproven.
