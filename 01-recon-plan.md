# 01 — RG405V recon plan (read-only first)

Goal: a complete, evidence-tagged hardware/firmware profile **before** any
modification. Nothing here writes to the device except where explicitly marked.

Legend: [seen] = captured output · [inferred] = reasoning · [unknown] = to probe.

---

## 0. Pre-flight

- [ ] Note the exact model on the back: RG405V, colour, serial.
- [ ] Record the stock firmware version from Settings → About.
- [ ] Enable Developer options → USB debugging.
- [ ] Install `adb` + `fastboot`; confirm `adb devices`.
- [ ] Check whether the bootloader is already unlocked (boot screen / `fastboot
      oem device-info` if available).
- [ ] Do **not** accept an OTA before dumping the current firmware.

> Anbernic warns that a third-party OS may block returning to the official OS.
> Treat the first flash as one-way until a verified backup exists.

---

## 1. Android / build inventory

Batch one shell call, cap the output:

```sh
adb shell '
echo "--- identity"; getprop | grep -E "ro.product.(model|device|name)|ro.build.(display.id|version.release|fingerprint|date)|ro.board.platform|ro.hardware";
echo "--- soc"; cat /proc/cpuinfo | grep -E "Hardware|Processor|model name" | head;
echo "--- kernel"; uname -a;
echo "--- abi"; getprop ro.product.cpu.abi; getprop ro.product.cpu.abilist;
echo "--- treble"; getprop ro.treble.enabled; getprop ro.build.type;
echo "--- slots"; getprop ro.boot.slot_suffix; ls /dev/block/by-name | head -40;
echo "--- memory"; grep -E "MemTotal|SwapTotal" /proc/meminfo;
echo "--- display"; wm size; wm density;
'
```

Record in the evidence log.

---

## 2. Partition map (read-only)

```sh
adb shell 'ls -l /dev/block/by-name/'
adb shell 'cat /proc/partitions'
adb shell 'lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINT'   # if toybox lsblk exists
adb shell 'mount | grep -E "system|vendor|product|data"'
```

Classify:

- A/B or A-only? (`ro.boot.slot_suffix`, `by-name` suffixes `_a/_b`)
- Dynamic partitions (`super`)? (`dmctl`, `lpdump` if available)
- Where is userdata? How big is the free space?
- Is `/vendor` a separate partition (Treble)?

---

## 3. Hardware probes

```sh
adb shell '
echo "--- thermal"; ls /sys/class/thermal/; for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/type) $(cat $z/temp)"; done;
echo "--- cpufreq"; ls /sys/devices/system/cpu/cpufreq/; cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors; cat /sys/devices/system/cpu/cpufreq/policy*/scaling_available_frequencies;
echo "--- devfreq"; ls /sys/class/devfreq/; for d in /sys/class/devfreq/*; do echo "$d"; cat $d/available_frequencies 2>/dev/null; done;
echo "--- battery"; dumpsys battery;
echo "--- input"; getevent -pl 2>/dev/null | head -80;
echo "--- display"; dumpsys SurfaceFlinger | grep -iE "display|mode" | head;
echo "--- wifi"; dumpsys wifi | grep -iE "mWifiInfo|SSID|supplicant" | head;
'
```

Notes to capture:

- CPU clusters (little/big), available governors, current `scaling_max_freq`.
- GPU devfreq path (Mali) and frequency table.
- Thermal trip points and throttle behavior.
- Input devices and axis/button codes (for andr36oid remaps).
- Panel resolution/refresh and rotation.

---

## 4. Root / ADB posture

```sh
adb shell 'which su; id'
adb shell 'getprop ro.debuggable; getprop ro.secure'
adb shell 'ls /data/adb 2>/dev/null'
```

- Is Magisk present? If yes, record version and modules.
- If not, decide: GammaOS Next (rooted by design) vs rooting stock.
- Wireless ADB persistence (if root): `service.adb.tcp.port 5555` via
  `/data/adb/service.d/` — same technique as the RG353V
  (`../rg353v-internals/04-wireless-adb.md`).

---

## 5. Bootloader / flashing path (research before writing)

For Unisoc T618, verify on the actual device:

- [ ] `fastboot devices` and `fastboot getvar all` (dump to a file).
- [ ] Does the device expose fastbootd / recovery ADB?
- [ ] Unlock command used by GammaOS on RG405M/V (read the official installer).
- [ ] Unisoc SP Flash Tool / `spd_dump` compatibility (loader version).
- [ ] Where is the official firmware available (Anbernic support page)?
- [ ] Is there a known "unbrick" loader/maskrom path?

Record the exact tool + version. Do not experiment with random Unisoc loaders.

---

## 6. Backup (mandatory before flashing)

Preferred: full partition dump via root or fastboot.

```sh
# example only — adapt to the actual partition list
adb shell 'dd if=/dev/block/by-name/boot of=/sdcard/boot.img'
adb pull /sdcard/boot.img
# repeat for vendor_boot, dtbo, vbmeta, super (if possible), persist, modem...
sha256sum *.img > SHA256SUMS
```

If the device is not rooted and fastboot cannot read partitions, **stop** and
find a verified vendor image + rollback procedure before flashing.

---

## 7. GammaOS Next evaluation

- [ ] Download the official GammaOS Next RG405M/V/RG505 v1.1 package.
- [ ] Read its README/installer: exact commands, unlock assumptions, data wipe.
- [ ] Check whether Android 13 or 14 is used for this device.
- [ ] Verify the package's checksums.
- [ ] After install: repeat §1–§3 and diff against stock (what changed).
- [ ] Test: Wi-Fi/BT, audio, touch, sticks/buttons, charging, sleep, HDMI/USB.

---

## 8. andr36oid layer port checklist

| Area | What to do | Reference |
|---|---|---|
| Performance | CPU/GPU floors, governor, `gamemode` equivalent | `../rgb10max2-internals/06-android-internals.md`, `../rg353v-internals/05-optimization.md` |
| Memory | zram tuning, swappiness, readahead | RGB10 docs |
| Debloat | package list review (GammaOS already debloated) | RGB10 docs |
| Launcher | Daijisho / Beacon / ES-like front-end | RG353 docs |
| Wireless ADB | persistent property via root | `../rg353v-internals/04-wireless-adb.md` |
| USB modes | gadget/OTG mode where applicable | RGB10 USB notes |
| Streaming | Tailscale + Moonlight | RGB10 docs |
| Input | remap axes/buttons; mouse mode | RGB10 input docs |
| Janitor | periodic `am kill-all` | RGB10/RG353 docs |

Each item must be re-probed on T618 — do not copy sysfs paths blindly.

---

## 9. Evidence log

Append blocks as findings arrive (template in `../docs/RE-METHOD.md` §3):

```md
### RE-YYYYMMDD-NN — <question>
- Target:
- Method:
- Seen:
- Inferred:
- Unknown:
- Next probe:
```
