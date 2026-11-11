#!/system/bin/sh
# 51-adb-wifi.sh — re-enable ADB over TCP (port 5555) after boot.
# Lives in /data/adb/service.d/ (Magisk late-start, runs as root).
#
# NOTE: this listens on the LAN. Disable the script if you do not want that.

sleep 20

setprop service.adb.tcp.port 5555
stop adbd 2>/dev/null
sleep 1
start adbd 2>/dev/null
