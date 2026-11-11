#!/system/bin/sh
# hid-gadget.sh — switch the RG405V USB-C port into a HID gamepad gadget.
#
# The Unisoc kernel has CONFIG_USB_CONFIGFS_F_HID=y, but Android's USB HAL only
# exposes mtp/ptp/rndis/midi/mass/adb. This script unbinds Android's gadget (g1),
# creates a second gadget (g2) with hid.usb0, binds it, and runs hidpad to feed
# it from the built-in controller.
#
#   start [seconds]  -> switch to HID; optional watchdog restores Android after N s
#   stop             -> tear down HID and rebind Android's gadget (adb/MTP back)
#   status           -> show gadget + daemon state
#
# While HID is active USB ADB/MTP are gone by design — control the device over
# wireless ADB, or just wait for the watchdog / reboot.

set -u

G=/config/usb_gadget/g2
G1=/config/usb_gadget/g1
UDC=musb-hdrc.1.auto
BIN=/data/local/tmp
DESC=$BIN/hid_desc.bin
LOGF=$BIN/hid-gadget.log

log() { echo "[$(date +%H:%M:%S)] $*" >> "$LOGF"; }

stop_gadget() {
    pkill -x hidpad 2>/dev/null
    if [ -d "$G" ]; then
        echo "" > "$G/UDC" 2>/dev/null
    fi
    sleep 1
    rm -rf "$G" 2>/dev/null
    start usb-hal-1-1 2>/dev/null
    sleep 1
    setprop sys.usb.config adb 2>/dev/null
    sleep 1
    log "stopped; Android gadget rebound"
}

start_gadget() {
    log "start"
    pkill -x hidpad 2>/dev/null
    if [ -d "$G" ]; then
        echo "" > "$G/UDC" 2>/dev/null
        rm -rf "$G" 2>/dev/null
    fi

    # Android's USB HAL re-binds g1 the moment it is unbound; stop it first.
    stop usb-hal-1-1 2>/dev/null
    setprop sys.usb.config none 2>/dev/null
    sleep 2
    if [ -n "$(cat "$G1/UDC" 2>/dev/null)" ]; then
        echo "" > "$G1/UDC" 2>/dev/null
        sleep 1
    fi
    log "g1 bound after unbind: '$(cat "$G1/UDC" 2>/dev/null)'"

    mkdir -p "$G/strings/0x409" "$G/configs/c.1/strings/0x409" "$G/functions/hid.usb0"

    echo 0x1209 > "$G/idVendor"
    echo 0x0002 > "$G/idProduct"
    echo 0x0100 > "$G/bcdDevice"
    echo 0x0200 > "$G/bcdUSB"
    echo 0x00   > "$G/bDeviceClass"
    echo 0x00   > "$G/bDeviceSubClass"
    echo 0x00   > "$G/bDeviceProtocol"

    echo "andr36oid"          > "$G/strings/0x409/manufacturer"
    echo "RG405V HID Gamepad" > "$G/strings/0x409/product"
    echo "RG405V0001"         > "$G/strings/0x409/serialnumber"
    echo "HID"                > "$G/configs/c.1/strings/0x409/configuration"
    echo 250                  > "$G/configs/c.1/MaxPower"

    echo 0 > "$G/functions/hid.usb0/protocol"
    echo 0 > "$G/functions/hid.usb0/subclass"
    echo 6 > "$G/functions/hid.usb0/report_length"
    cat "$DESC" > "$G/functions/hid.usb0/report_desc"
    ln -s "$G/functions/hid.usb0" "$G/configs/c.1/hid.usb0" 2>/dev/null

    if echo "$UDC" > "$G/UDC" 2>/dev/null; then
        sleep 1
        nohup "$BIN/hidpad" > "$BIN/hidpad.log" 2>&1 &
        log "bound $UDC with hid.usb0"
    else
        log "ERROR: bind failed; restoring Android"
        stop_gadget
        return 1
    fi
}

case "${1:-}" in
    start)
        secs="${2:-0}"
        start_gadget
        case "$secs" in
            ''|*[!0-9]*) secs=0 ;;
        esac
        if [ "$secs" -gt 0 ]; then
            ( sleep "$secs"; stop_gadget ) >/dev/null 2>&1 &
            log "watchdog: restore Android gadget in ${secs}s"
        fi
        ;;
    stop) stop_gadget ;;
    status)
        if [ -d "$G" ]; then echo "g2: present"; else echo "g2: absent"; fi
        [ -d "$G" ] && echo "g2 bound: $(cat "$G/UDC" 2>/dev/null)"
        echo "g1 bound: $(cat "$G1/UDC" 2>/dev/null)"
        echo "hidpad: $(pidof hidpad 2>/dev/null || echo stopped)"
        [ -e /dev/hidg0 ] && echo "/dev/hidg0: present" || echo "/dev/hidg0: absent"
        ;;
    *)
        echo "usage: $0 start [auto_restore_seconds] | stop | status"
        exit 2
        ;;
esac
