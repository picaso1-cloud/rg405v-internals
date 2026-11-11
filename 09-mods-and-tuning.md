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

## 5. D-pad in Android UI (Tab bridge)

**Problem:** `gammapad` forwards the D-pad only as **`ABS_HAT0X/Y`**. Games read
that fine, but AOSP *phone* Settings navigates with **Tab / Enter**, not arrows.
Verified on this build: a native `input keyevent 20` (DPAD_DOWN) focuses only
`com.android.settings:id/homepage_container`; the individual rows are reachable
with **Tab**.

**Fix:** [`tools/dpadkeys/dpadkeys.c`](tools/dpadkeys/dpadkeys.c) injects a
virtual keyboard:

| Input | Injected |
|---|---|
| D-pad Down | `TAB` |
| D-pad Up | `SHIFT+TAB` |
| D-pad Left / Right | `KEY_LEFT` / `KEY_RIGHT` |
| A | `ENTER` |
| B | `BACK` |

**One press = one move** in Tab mode (repeat is disabled; a held press used to
fire twice). Arrow mode keeps hold-to-repeat. The original hat events are still
delivered, so emulators keep their normal D-pad.

`/data/local/tmp/dpadkeys.conf`:

```ini
mode=tab          # tab (Android UI) or arrows
enter_on_a=1
back_on_b=1
a_east=0          # 0: A (BTN_SOUTH) = Enter, B (BTN_EAST) = Back
```

Autostart: [`tools/scripts/52-dpadkeys.sh`](tools/scripts/52-dpadkeys.sh) in
`/data/adb/service.d/`.

Result [seen]: in Settings the D-pad now steps **one row per press** through the
search bar, the category rows and the controls; A opens, B goes back.

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

## 9. Next steps

1. Insert the card into the phone; confirm `public:…` mounts and
   `/storage/<uuid>/ROMs` is readable.
2. Point Daijishō at the seven platforms; run one title per system.
3. Decide whether `51-adb-wifi.sh` should stay enabled (LAN exposure).
4. Commit the panel/tools to the repo (done) and keep the APK in `tools/controlpanel/out`.
5. Unbricker still pending — rollback unproven.
