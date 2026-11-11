#!/usr/bin/env python3
"""Unisoc bootloader unlock with a longer on-screen confirmation window."""
import base64
import io
import os
import sys

from unisoc_unlock.bundled_adb import fastboot, usb_exceptions
from Crypto.Signature import PKCS1_v1_5
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA

WAIT_MS = int(os.environ.get("UNLOCK_WAIT_MS", "240000"))


class OemIdToken:
    def __init__(self):
        self.n = 0
        self.id = None

    def __call__(self, fb_msg):
        if self.n == 1 and self.id is None and fb_msg.header == b"INFO":
            self.id = fb_msg.message.decode("utf-8").strip()
        self.n += 1


def sign_token(tok, key_file):
    priv = RSA.importKey(open(key_file).read())
    h = SHA256.new(tok)
    return PKCS1_v1_5.new(priv).sign(h)


def main():
    print("connecting to fastboot device...", flush=True)
    dev = fastboot.FastbootCommands()
    dev.ConnectDevice()

    oem = OemIdToken()
    print("requesting OEM identifier token...", flush=True)
    dev.Oem("get_identifier_token", info_cb=oem)
    print("OEM ID:", oem.id, flush=True)
    if not oem.id:
        print("ERROR: no OEM ID returned", file=sys.stderr)
        sys.exit(2)

    raw = base64.b16decode(oem.id.ljust(128, "0"), casefold=True)
    pem = os.path.join(os.path.dirname(__import__("unisoc_unlock").__file__), "rsa4096_vbmeta.pem")
    sig = sign_token(raw, pem)

    print("uploading signature...", flush=True)
    dev.Download(io.BytesIO(sig), source_len=len(sig))

    print(f"UNLOCK COMMAND SENT - press Home/Back on the device NOW ({WAIT_MS//1000}s window)", flush=True)
    try:
        dev._SimpleCommand(b"flashing unlock_bootloader", timeout_ms=WAIT_MS)
    except Exception as exc:
        print("UNLOCK FAILED:", type(exc).__name__, exc, file=sys.stderr, flush=True)
        try:
            dev.Close()
        except Exception:
            pass
        sys.exit(3)

    print("BOOTLOADER UNLOCKED", flush=True)
    dev.Close()


if __name__ == "__main__":
    main()
