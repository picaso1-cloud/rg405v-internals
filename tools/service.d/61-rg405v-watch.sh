#!/system/bin/sh
# Magisk service.d entry — start the RG405V telemetry watchdog after boot.
# Install:
#   adb push 61-rg405v-watch.sh /data/adb/service.d/61-rg405v-watch.sh
#   adb shell chmod 755 /data/adb/service.d/61-rg405v-watch.sh
#
# A short delay avoids competing with boot/provisioning I/O.
(
  sleep 30
  [ -x /data/adb/rg405v-watch.sh ] && /data/adb/rg405v-watch.sh start
) >/dev/null 2>&1 &
