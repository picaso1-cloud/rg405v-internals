#!/system/bin/sh
# rg405v-watch.sh — low-overhead telemetry for the RG405V.
#
# Samples a compact snapshot every WATCH_INTERVAL seconds (default 600).
# Writes a machine-readable batterystats CSV every WATCH_BATT_EVERY samples
# (default 36 = ~6 h). Logs rotate at WATCH_MAX_LOG (default 5 MiB), keeping
# watch.log.1 … watch.log.4.
#
# Usage:
#   rg405v-watch.sh once      # one sample now
#   rg405v-watch.sh start     # daemonize (also used by service.d)
#   rg405v-watch.sh stop
#   rg405v-watch.sh status
#   rg405v-watch.sh tail
#
# Install:
#   adb push rg405v-watch.sh /data/adb/rg405v-watch.sh
#   adb shell chmod 755 /data/adb/rg405v-watch.sh
#   adb shell /data/adb/rg405v-watch.sh once
#   adb shell /data/adb/rg405v-watch.sh start
#
# Pull:
#   adb root && adb pull /data/adb/rg405v-watch ./rg405v-watch-$(date +%F)

DIR=${WATCH_DIR:-/data/adb/rg405v-watch}
LOG=$DIR/watch.log
PID=$DIR/watch.pid
INTERVAL=${WATCH_INTERVAL:-600}
BATT_EVERY=${WATCH_BATT_EVERY:-36}
MAX_LOG=${WATCH_MAX_LOG:-5242880}

mkdir -p "$DIR" 2>/dev/null

rotate() {
  [ -f "$LOG" ] || return 0
  size=$(wc -c < "$LOG" 2>/dev/null)
  [ "${size:-0}" -lt "$MAX_LOG" ] && return 0
  [ -f "$LOG.3" ] && mv -f "$LOG.3" "$LOG.4"
  [ -f "$LOG.2" ] && mv -f "$LOG.2" "$LOG.3"
  [ -f "$LOG.1" ] && mv -f "$LOG.1" "$LOG.2"
  mv -f "$LOG" "$LOG.1"
}

snap() {
  rotate
  {
    echo "===== $(date '+%Y-%m-%d %H:%M:%S %z') uptime=$(cut -d. -f1 /proc/uptime 2>/dev/null)s ====="
    echo "-- battery"
    dumpsys battery 2>/dev/null | grep -E 'AC powered|USB powered|Wireless powered|status:|health:|level:|voltage:|temperature:|Charge counter:'
    for p in /sys/class/power_supply/battery /sys/class/power_supply/sc27xx-fgu; do
      [ -d "$p" ] || continue
      echo "$(basename "$p"): capacity=$(cat "$p/capacity" 2>/dev/null) current_now=$(cat "$p/current_now" 2>/dev/null)"
    done
    echo "-- power"
    dumpsys power 2>/dev/null | grep -E 'mWakefulness=|mIsPowered=|mLastWakeTime=|mLastSleepTime='
    dumpsys power 2>/dev/null | sed -n '/Wake Locks:/,/^$/p' | head -20
    echo "-- idle"
    dumpsys deviceidle 2>/dev/null | grep -E 'mState=|mLightState=|mScreenOn=|mCharging=|mNetworkConnected=|mNotMoving=|mLightEnabled=|mDeepEnabled='
    echo "-- memory"
    grep -E 'MemTotal|MemFree|MemAvailable|Cached:|SwapTotal|SwapFree' /proc/meminfo
    echo "zram_mm_stat: $(cat /sys/block/zram0/mm_stat 2>/dev/null)"
    echo "-- psi"
    cat /proc/pressure/cpu /proc/pressure/memory /proc/pressure/io 2>/dev/null
    echo "-- top"
    top -b -n1 2>/dev/null | head -14
    echo "-- thermal"
    dumpsys thermalservice 2>/dev/null | grep -m1 'Thermal Status'
    for z in /sys/class/thermal/thermal_zone*; do
      t=$(cat "$z/type" 2>/dev/null)
      case "$t" in
        soc-thmzone|cputop-thmzone|gpuank2-thmzone|board-thmzone)
          echo "$t=$(cat "$z/temp" 2>/dev/null)" ;;
      esac
    done
    echo "-- clocks"
    for p in /sys/devices/system/cpu/cpufreq/policy0 /sys/devices/system/cpu/cpufreq/policy6; do
      [ -d "$p" ] || continue
      echo "$(basename "$p") gov=$(cat "$p/scaling_governor" 2>/dev/null) cur=$(cat "$p/scaling_cur_freq" 2>/dev/null)"
    done
    echo "gpu_cur=$(cat /sys/class/devfreq/60000000.gpu/cur_freq 2>/dev/null)"
    echo "fan mode=$(getprop persist.gammaos.fan_mode) pwm=$(cat /sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm1 2>/dev/null) en=$(cat /sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm_en 2>/dev/null) power=$(cat /sys/devices/platform/singleadc-joypad/fan_power 2>/dev/null)"
    echo "-- foreground"
    dumpsys activity activities 2>/dev/null | grep -m1 topResumedActivity
    echo "-- alarms"
    dumpsys alarm 2>/dev/null | grep -E 'Pending alarms per uid'
    echo
  } >> "$LOG" 2>&1

  n=$(cat "$DIR/.count" 2>/dev/null)
  n=$(( ${n:-0} + 1 ))
  echo "$n" > "$DIR/.count"
  if [ $((n % BATT_EVERY)) -eq 0 ]; then
    timeout 60 dumpsys batterystats -c > "$DIR/batterystats-$(date '+%Y%m%d-%H%M').csv" 2>/dev/null
    ls -1t "$DIR"/batterystats-*.csv 2>/dev/null | tail -n +9 | while read -r f; do rm -f "$f"; done
  fi
}

loop() {
  echo $$ > "$PID"
  trap 'rm -f "$PID"; exit 0' INT TERM
  while :; do
    snap
    sleep "$INTERVAL"
  done
}

case "$1" in
  once)
    snap
    echo "sample written to $LOG"
    ;;
  loop)
    loop
    ;;
  start)
    if [ -f "$PID" ] && kill -0 "$(cat "$PID" 2>/dev/null)" 2>/dev/null; then
      echo "already running: $(cat "$PID")"
      exit 0
    fi
    nohup setsid "$0" loop >/dev/null 2>&1 &
    sleep 1
    echo "started: $(cat "$PID" 2>/dev/null)"
    ;;
  stop)
    [ -f "$PID" ] && kill "$(cat "$PID" 2>/dev/null)" 2>/dev/null
    rm -f "$PID"
    echo "stopped"
    ;;
  status)
    if [ -f "$PID" ] && kill -0 "$(cat "$PID" 2>/dev/null)" 2>/dev/null; then
      echo "running pid=$(cat "$PID") interval=${INTERVAL}s log=$LOG"
    else
      echo "not running"
    fi
    [ -f "$LOG" ] && tail -n 3 "$LOG"
    ls -lh "$DIR" 2>/dev/null
    ;;
  tail)
    tail -n 40 "$LOG"
    ;;
  *)
    echo "usage: $0 once|start|stop|status|tail"
    exit 2
    ;;
esac
