# 07 — Bootloader: unlock, verify, lock and unbrick (Unisoc T618)

**Date:** 2026-11-11 · **Device:** Anbernic RG405V (`ums512_1h10`, Unisoc Tiger
T618) · **Scope:** the only write path into internal storage.

Evidence tags: **[seen]** observed on this unit · **[inferred]** official
docs / vendor tooling · **[unknown]** not verified here.

> **State on 2026-11-11: bootloader UNLOCKED, stock Android 12 V1.23, user data
> wiped, GammaOS Next not yet flashed.** The unlock was done on Linux with
> `unisoc-unlock`; the official unbricker was **downloaded and verified on 2026-11-12** (2,549,946,021 bytes; SHA-256 `9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71`; `7z t` OK).

---

## 1. State machine

| State | How to recognise | Notes |
|---|---|---|
| **Android + ADB** | `adb devices` → `79783730379533  device` | normal boot |
| **Android, no ADB** | USB `1782:4001`, interface class 6 (Imaging/MTP) | happens right after an unlock wipe: USB debugging is off [seen] |
| **fastboot** | `fastboot devices` → `79783730379533  fastboot`; screen shows fastboot next to the Anbernic logo | bootloader; where unlocking happens |
| **fastbootd** | screen shows large red **`fastbootd`**; `fastboot devices` also lists it | userspace fastboot; where GammaOS partitions are written |
| **SPD download / BROM** | USB `1782:…`, no ADB/fastboot | Unisoc flashing mode used by the unbricker [inferred] |

Entries:

```sh
adb reboot bootloader     # Android -> fastboot
fastboot reboot fastboot  # fastboot -> fastbootd
adb reboot fastboot       # Android -> fastbootd directly (needs adb)
```

`fastboot reboot` from fastboot/fastbootd returns to Android.

---

## 2. Preflight

- [ ] **Battery ≥ 50 %** (was 100 %, `dumpsys battery`).
- [ ] **microSD removed.** Pulling/inserting a card while in the bootloader can
      reset the device out of fastboot — observed once: the unit dropped back to
      Android while the card was being removed [seen; cause inferred].
- [ ] `adb`, `fastboot` (platform-tools), `7z`.
- [ ] `python3 -m venv` + `pip install unisoc-unlock`.
- [ ] A **known-good USB data cable**; the unit enumerates on a USB-2 bus at
      `high-speed` — that is normal for this bootloader [seen].
- [x] **Unbricker downloaded/verified** 2026-11-12 (2,549,946,021 bytes; SHA-256 `9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71`).

Build/tool versions used: Android 12 **V1.23**, `fastboot 35.0.2`,
`unisoc-unlock 0.0.2`.

---

## 3. Unlock the bootloader — verified procedure

Unlocking **factory-resets the device** (apps, `/sdcard`, settings). The
bootloader flag itself is a tiny write; the reset is what follows.

### 3.1 Linux / macOS — `unisoc-unlock` (path used here)

```sh
python3 -m venv ~/.venvs/unisoc
~/.venvs/unisoc/bin/pip install unisoc-unlock

adb reboot bootloader
until fastboot devices | grep -q 79783730379533; do sleep 1; done   # ~10–50 s

~/.venvs/unisoc/bin/python -m unisoc_unlock unlock
```

What the tool does [seen in source]:

1. `fastboot oem get_identifier_token` → the device returns a token that is just
   the **hex-encoded serial number** (`3739373833373330333739353333` =
   `79783730379533`).
2. Signs it with the bundled `rsa4096_vbmeta.pem` and uploads the signature.
3. Sends `flashing unlock_bootloader` and waits for the on-screen confirmation.

**On the device:** a confirmation prompt appears. Press **Home / Back** —
**not** Volume Down. (Same wording as the GammaOS wiki.)

### 3.2 The 60-second trap (important)

`unisoc_unlock` hard-codes a **60 s** ACK timeout. If the button is not pressed
in time it raises:

```
ReadFailedError: Could not receive data from first (timeout 60000ms): LIBUSB_ERROR_TIMEOUT [-7]
```

**The unlock may still have been applied.** In our run it was: the device
applied the flag, wiped, and rebooted. Retrying then returns:

```
FAIL: Bootloader can not been unlocked repeatly.
```

That message means **already unlocked**, not "failed". Do not keep retrying; go
to §4 and verify from Android.

If you want a larger window, `tools/unisoc_unlock_long.py` in this repo is the
same flow with `UNLOCK_WAIT_MS` (default 240000):

```sh
UNLOCK_WAIT_MS=240000 python tools/unisoc_unlock_long.py
```

### 3.3 Chrome alternative — `subut-rehost`

Windows/macOS users, or no Python: open
<https://thegammasqueeze.github.io/subut-rehost/> in Chrome/Chromium, connect
the device while it is in **fastboot**, then **Connect → Unlock** and confirm on
the device with Home/Back. Same result; needs WebUSB permission.

### 3.4 What turned out **not** to be required

- The **"OEM unlocking" developer toggle** was left **off**
  (`sys.oem_unlock_allowed=0`) and the unlock still succeeded — the token/signature
  path bypasses it. `ro.oem_unlock_supported=1`.
- Plain `fastboot flashing unlock` is not the mechanism on this bootloader;
  `fastboot oem device-info` / `getvar unlocked` are **not implemented**
  (`unknown cmd.` / empty output).

---

## 4. Verify the unlock

There is no usable `fastboot getvar` for this. Check Android props:

```sh
adb shell getprop ro.boot.flash.locked        # 1 = locked, 0 = unlocked
adb shell getprop ro.boot.verifiedbootstate   # green = stock, orange = unlocked
adb shell getprop ro.boot.veritymode          # enforcing
```

Observed transition [seen]:

| Prop | Stock (before) | Unlocked (after) |
|---|---|---|
| `ro.boot.flash.locked` | `1` | **`0`** |
| `ro.boot.verifiedbootstate` | `green` | **`orange`** |
| `ro.boot.veritymode` | `enforcing` | `enforcing` |

Cosmetic confirmation: on boot the screen briefly prints
`INFO: LOCK FLAG IS : UNLOCK!!!` [inferred from the GammaOS wiki].

---

## 5. What happens right after the unlock

1. Device wipes data and reboots to the Android **setup wizard**.
2. **USB debugging is off** — `adb` disappears and the unit shows up as MTP
   (`1782:4001`, Imaging class). This is expected, not a brick [seen].
3. Re-enable it: finish setup → **About phone → tap Build number 7×** →
   **Developer options → USB debugging** → accept the RSA prompt.
4. The Anbernic preload apps are reinstalled into `/data` on first boot, so
   `pm list packages -3` shows ~28 entries even on a fresh install [seen].
5. Daijishō configuration, emulator settings and saves from the stock install
   are **gone** (they lived in internal storage). The microSD library is
   untouched because the card was out.

---

## 6. Lock the bootloader (usually a bad idea)

```sh
adb reboot bootloader
~/.venvs/unisoc/bin/python -m unisoc_unlock lock
```

**Only lock after flashing a fully stock, signed image** (official unbricker
restore). Locking with custom/modified partitions installed makes verified boot
fail and the device will not boot; relocking also tends to trigger a wipe
[inferred].

There is no reason to relock for daily use: the unlock state only adds an
`orange` boot warning. Keep it unlocked while GammaOS / andr36oid work is in
progress.

---

## 7. Unbrick / rollback

### 7.1 The official unbricker

GammaOS wiki → *Stock Firmware / Unbricking (Optional)* → **RG405V**:

| Item | Value |
|---|---|
| File | `RG405V_UNBRICKER_SPD_Upgrade_Tool_R26.21.2801.7z` |
| Size | `2,549,946,021` bytes (~2.4 GiB) |
| Source | <https://drive.google.com/file/d/1idODOwE197mRHwMHDU_Y7bnAiWiackbN/view> |
| Tool | Unisoc **SPD Upgrade Tool** (`UpgradeDownload.exe`) **[inferred]** |
| Platform | **Windows** (no verified Linux path yet) |

There is **no userspace way to dump the stock eMMC** (no root, locked verity):
the unbricker is the only rollback. Treat the first `FlashPartitions.sh` as
one-way until the unbricker has been run successfully at least once.

### 7.2 Restore procedure (documented flow, not yet executed here)

1. Extract the `.7z` with 7-Zip (no Patreon password for this archive
   **[unknown]** — the release archives are password-protected, the unbricker is
   not expected to be).
2. Install the Unisoc/Spreadtrum USB drivers on the Windows host.
3. Run `UpgradeDownload.exe`, load the `.pac` for **RG405V** (not 405M/505).
4. Click start, then connect the **powered-off** device while holding the
   download-mode key combo.
5. Wait for `Passed`; the device reboots into stock Android 12.

**Download-mode key combo for the RG405V: not verified.** Common Unisoc
patterns are *hold Volume +/− while plugging in*; the GammaOS T820 guide uses
*hold Back while plugging in*. Try both; the device must appear as an SPD/BROM
USB device, not as fastboot or MTP.

### 7.3 Linux option

`spd_dump` (open-source Unisoc SPL tool) can write partitions over BROM/SPL, but
**it has not been tested with this `.pac`** and the PAC container needs
unpacking first. Status: **[unknown] — do not rely on it for the first flash.**
A Windows VM with USB passthrough is the safer route.

---

## 8. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `No device found: No device available, or it is in the wrong configuration` | device left fastboot (e.g. card removal), or not in fastboot | re-run `adb reboot bootloader`, wait for `fastboot devices`, retry immediately |
| `LIBUSB_ERROR_TIMEOUT [-7]` during unlock | 60 s confirmation window elapsed | verify state (§4); if still locked, retry with `tools/unisoc_unlock_long.py` |
| `FAIL: Bootloader can not been unlocked repeatly.` | already unlocked | nothing to do; verify §4 |
| `adb devices` empty right after unlock | USB debugging reset by the wipe | finish setup, re-enable USB debugging |
| USB shows `1782:4001` | MTP/PTP, Android booted without ADB | same as above |
| `fastboot oem device-info` → `unknown cmd.` | not implemented | use §4 props |
| Device drops out of fastboot | idle timeout / card insertion | re-enter; keep the card out |

---

## 9. Command shelf

```sh
# state
adb devices; fastboot devices
adb shell getprop ro.boot.flash.locked
adb shell getprop ro.boot.verifiedbootstate

# modes
adb reboot bootloader
fastboot reboot fastboot
fastboot reboot

# unlock / lock
~/.venvs/unisoc/bin/python -m unisoc_unlock unlock
~/.venvs/unisoc/bin/python -m unisoc_unlock lock

# gammaos flash (fastbootd, card out)
sh FlashPartitions.sh
sh EraseUserData.sh
```

---

## 10. Open questions

1. Exact **download-mode button combo** on the RG405V (§7.2).
2. Whether the unbricker **relocks** the bootloader and restores a green
   `verifiedbootstate`.
3. A **Linux** flash/restore path (`spd_dump` + PAC unpack) — verify on a spare
   device before trusting it.
4. Does GammaOS Next's `FlashPartitions.sh` need `fastbootd` explicitly, or does
   it switch modes itself? (Will be recorded when the flash is executed.)
