# RG405V — recovery and rollback

Two different jobs, two different tools. Do not mix them up.

| Situation | First-line tool | Why |
|---|---|---|
| Android is broken but **fastboot/fastbootd still works** | **GammaOS Next v1.1 Lite** reflash | same image already flashed successfully; local and verified |
| Bad system/vendor/boot flash, bootloop, encryption/data mess | **GammaOS** reflash + `EraseUserData.sh` | writes the Android partitions and wipes data |
| Device only appears in **BROM/SPD download mode**, no fastboot | **Official RG405V unbricker** | GammaOS cannot write `lk`/GPT/trustos |
| Need **stock** firmware, warranty or relock | **Official unbricker** | GammaOS restores GammaOS, not stock |
| Corrupted bootloader / partition table | **Official unbricker** | only SPD path reaches the device |

## 1. GammaOS Next reflash — tested, first-line

Package (already downloaded and verified):

```
/home/souhail/rg405v-gammaos/
  RG405V_GammaOSNext_Lite_v1.1.7z.001…005   (2.42 GB, 7z test OK)
  extracted/RG405V_GammaOSNext_Lite_v1.1/
```

Previous flash log: `extracted/.../log.txt` — exit 0, 0 failures, ~6 min.

Procedure (fastbootd):

```sh
adb reboot fastboot
fastboot devices
fastboot getvar is-userspace     # is-userspace: yes

cd /home/souhail/rg405v-gammaos/extracted/RG405V_GammaOSNext_Lite_v1.1
bash FlashPartitions.sh          # vbmeta/boot/dtbo/vendor_boot/vendor/system
bash EraseUserData.sh            # userdata/metadata wipe + fastboot -w
fastboot reboot
```

What it writes: `vbmeta` (A/B), `boot` (A/B), `dtbo` (A/B), `vendor_boot`
(A/B), `vendor` (logical), `system` (logical). It does **not** write:
`lk`/`bootloader`, GPT, `trustos`, `misc` or the vendor low-level partitions.
That is why it is a soft-brick recovery, not a true unbrick.

Data: `EraseUserData.sh` wipes `/data`. ROMs on the SD card are untouched.
Restore the customisations from
`/home/souhail/rg405v-backup/custom-2026-11-12/` afterwards (check Magisk/root
is active before pushing `service.d`).

## 2. Official RG405V unbricker — hard-brick / stock gate

Downloaded and verified 2026-11-12:

```
/home/souhail/rg405v-gammaos/RG405V_UNBRICKER_SPD_Upgrade_Tool_R26.21.2801.7z
  size   2,549,946,021 bytes
  sha256 9212eec4274da0bd811a776078b9636e56af01d62d4a7f97d00960e2796b3b71
  7z t   OK (18 folders, 115 files, 6,734,913,057 bytes uncompressed)
```

Tool: Unisoc SPD Upgrade Tool (`UpgradeDownload.exe`, Windows). The device must
be in SPD/BROM download mode. No verified Linux path. This is the only rollback
that restores **stock** or recovers a bootloader/GPT brick.

## 3. Practical rule

- Try **GammaOS reflash** first whenever `fastboot devices` shows the device.
- Keep the **unbricker** for the cases where fastboot is gone or stock is
  required. It is downloaded/verified; keep it next to the GammaOS package.
- Both are local; no network needed for recovery.
- After either path, verify:
  `wm size` (640x480 native), `getprop ro.build.version.release` (14),
  `adb shell cmd wifi status`, and re-apply the customisation backup.

## 4. Test the path before you need it

A recovery tool is only proven when it has been run at least once. When the
device has nothing important on it:

1. Back up `/data` customisations (already done).
2. Run the GammaOS `FlashPartitions.sh` + `EraseUserData.sh` again.
3. Confirm it boots and the panel/Wi-Fi/customisations restore.
4. If a Windows VM is available, test the SPD unbricker on a spare/known state
   before relying on it.
