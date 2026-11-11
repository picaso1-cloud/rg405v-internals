#!/bin/bash
# rg405v-log.sh — keep the sprd-wlan kernel flood out of normal debugging.
#
# The Unisoc Wi-Fi driver logs every management frame to the *kernel* buffer
# (tag: sprd-wlan). During association it can emit tens of thousands of lines
# and rotate away the app logs. This helper captures two separate streams:
#   app-*.log    main + system + crash  (no kernel, no sprd-wlan)
#   wifi-*.log   kernel buffer, only sprd-wlan lines
#
# Usage:
#   ./rg405v-log.sh start        start both captures in the background
#   ./rg405v-log.sh stop         stop them
#   ./rg405v-log.sh status       show PIDs, buffer sizes, line counts
#   ./rg405v-log.sh app [N]      print the last N app lines (default 2000)
#   ./rg405v-log.sh wifi [N]     print the last N sprd-wlan lines (default 300)
#   ./rg405v-log.sh snapshot     write both to timestamped files, print paths
#
# Device serial: set ADB_SERIAL, default is the RG405V.
set -u
D="${ADB_SERIAL:-79783730379533}"
OUT="${OUT:-/home/souhail/Andr36oid/logs/rg405v}"
mkdir -p "$OUT"
TS() { date +%Y%m%d-%H%M%S; }
ADB="adb -s $D"

case "${1:-status}" in
  start)
    $ADB logcat -c 2>/dev/null
    app="$OUT/app-$(TS).log"; wifi="$OUT/wifi-$(TS).log"
    nohup $ADB logcat -b main -b system -b crash -v threadtime >"$app" 2>&1 &
    echo $! > "$OUT/app.pid"
    nohup bash -c "$ADB logcat -b kernel -v threadtime | grep --line-buffered sprd-wlan" >"$wifi" 2>&1 &
    echo $! > "$OUT/wifi.pid"
    echo "app  -> $app"
    echo "wifi -> $wifi"
    ;;
  stop)
    for f in app wifi; do
      [ -f "$OUT/$f.pid" ] && kill "$(cat "$OUT/$f.pid")" 2>/dev/null
      rm -f "$OUT/$f.pid"
    done
    pkill -f "adb -s $D logcat" 2>/dev/null
    echo "stopped"
    ;;
  status)
    echo "serial: $D"
    $ADB shell "logcat -g" 2>/dev/null | head -4
    for f in app wifi; do
      [ -f "$OUT/$f.pid" ] && echo "$f pid $(cat "$OUT/$f.pid")" || echo "$f not running"
    done
    ls -1t "$OUT"/*.log 2>/dev/null | head -4
    ;;
  app)
    $ADB logcat -d -b main -b system -b crash -v threadtime 2>/dev/null | tail -n "${2:-2000}"
    ;;
  wifi)
    $ADB logcat -d -b kernel -v threadtime 2>/dev/null | grep sprd-wlan | tail -n "${2:-300}"
    ;;
  snapshot)
    a="$OUT/app-$(TS).log"; w="$OUT/wifi-$(TS).log"
    $ADB logcat -d -b main -b system -b crash -v threadtime >"$a" 2>/dev/null
    $ADB logcat -d -b kernel -v threadtime 2>/dev/null | grep sprd-wlan >"$w"
    echo "$a"
    echo "$w"
    ;;
  *)
    sed -n '2,20p' "$0"
    ;;
esac
