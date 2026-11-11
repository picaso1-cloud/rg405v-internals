# Device scripts (pulled 2026-11-12)

Copies of the live RG405V scripts/configs. The full backup, binaries, Panel APK,
Daijishō appdata and settings dumps are at:

`/home/souhail/rg405v-backup/custom-2026-11-12/`

- `50-mousemode.sh`, `51-adb-wifi.sh`, `60-log-tuning.sh` → `/data/adb/service.d/`
- `52-dpadkeys.sh` → retired; D-pad is native
- `usbmode.sh`, `perfmode.sh`, `hid-gadget.sh`, `mousemode.conf`,
  `dpadkeys.conf`, `hidpad.c` → `/data/local/tmp/` / build sources
