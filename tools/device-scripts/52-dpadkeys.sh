#!/system/bin/sh
# 52-dpadkeys.sh — start the D-pad -> Tab/Enter/Back bridge at boot.
# Lives in /data/adb/service.d/ (Magisk late-start, runs as root).

sleep 28

[ -x /data/local/tmp/dpadkeys ] || exit 0

if ! pidof dpadkeys >/dev/null 2>&1; then
    nohup /data/local/tmp/dpadkeys >> /data/local/tmp/dpadkeys.log 2>&1 &
fi
