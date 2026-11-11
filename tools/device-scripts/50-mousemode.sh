#!/system/bin/sh
# 50-mousemode.sh — start the gamepad->pointer daemon at boot.
# Lives in /data/adb/service.d/ (Magisk late-start, runs as root).
# Toggle on the device: hold L1 + R1 + Select.

sleep 25

[ -x /data/local/tmp/mousemode ] || exit 0

if ! pidof mousemode >/dev/null 2>&1; then
    nohup /data/local/tmp/mousemode >> /data/local/tmp/mousemode.log 2>&1 &
fi
