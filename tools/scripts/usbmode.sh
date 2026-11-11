#!/system/bin/sh
# usbmode.sh — switch the RG405V USB-C port function.
#
#   usbmode.sh mtp        file transfer
#   usbmode.sh ptp        camera transfer
#   usbmode.sh rndis      USB tethering (network)
#   usbmode.sh midi       MIDI
#   usbmode.sh adb        ADB only
#   usbmode.sh charging   charge only (no data)
#   usbmode.sh status     show current config
#
# The vendor init also supports "mass" (USB mass storage), but Android 14
# exposes it through the USB HAL, so it may be refused depending on the build.

case "${1:-}" in
    mtp|ptp|rndis|midi|adb)
        svc usb setFunctions "$1"
        echo "usb functions -> $1"
        ;;
    charging|none)
        svc usb setFunctions
        echo "usb functions -> charging only"
        ;;
    status)
        echo "sys.usb.config = $(getprop sys.usb.config)"
        echo "sys.usb.state  = $(getprop sys.usb.state)"
        ;;
    *)
        echo "usage: $0 mtp|ptp|rndis|midi|adb|charging|status"
        exit 2
        ;;
esac
