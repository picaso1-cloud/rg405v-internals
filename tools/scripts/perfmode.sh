#!/system/bin/sh
# perfmode.sh — switch the GammaOS clock profile.
#
#   perfmode.sh max        CPU/GPU pinned to maximum (benchmarking, gaming)
#   perfmode.sh stock      schedutil + simple_ondemand (default)
#   perfmode.sh powersave  minimum clocks (endurance)
#
# Persists across reboot (persist.gammaos.performance_mode).

case "${1:-}" in
    max|stock|powersave)
        setprop persist.gammaos.performance_mode "$1"
        sleep 1
        echo "performance_mode=$(getprop persist.gammaos.performance_mode)"
        ;;
    status)
        echo "performance_mode=$(getprop persist.gammaos.performance_mode)"
        for d in /sys/devices/system/cpu/cpufreq/policy*; do
            echo "  $(basename $d): $(cat $d/scaling_governor) @ $(cat $d/scaling_cur_freq)"
        done
        echo "  gpu: $(cat /sys/devices/platform/soc/soc:mm/60000000.gpu/devfreq/60000000.gpu/governor) @ $(cat /sys/devices/platform/soc/soc:mm/60000000.gpu/devfreq/60000000.gpu/cur_freq)"
        ;;
    *)
        echo "usage: $0 max|stock|powersave|status"
        exit 2
        ;;
esac
