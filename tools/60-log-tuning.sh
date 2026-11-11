#!/system/bin/sh
# 60-log-tuning.sh — keep logd usable despite the sprd-wlan kernel flood.
# Runs as root (Magisk late-start). Safe to remove at any time.
#
# Why: the Unisoc Wi-Fi driver logs every management frame to the kernel
# buffer. During association that can be tens of thousands of lines and it
# rotates away app/system logs. 256 KiB buffers are not enough.
sleep 20

LOGCAT=/system/bin/logcat
$LOGCAT -b main   -G 4M
$LOGCAT -b system -G 4M
$LOGCAT -b crash  -G 4M
$LOGCAT -b kernel -G 4M

# Persist for the next logd start as well.
setprop persist.logd.size 4M
setprop persist.logd.size.main 4M
setprop persist.logd.size.system 4M
setprop persist.logd.size.crash 4M
setprop persist.logd.size.kernel 4M

# Kernel messages ignore log.tag, but this keeps any userspace copy quiet.
setprop persist.log.tag.sprd-wlan S
