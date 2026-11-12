#!/usr/bin/env python3
"""Force ARMSX2's setMaliSX2Offered(true) gate on Mali-G52.

The nightly computes:

    NativeApp.setMaliSX2Offered(isMaliSX2Supported(gpu))

and isMaliSX2Supported() is false for Mali-G52. Replace the boolean it passes
with a constant true. This only changes the UI/native offered flag; the driver
still has to load and work, which MaliSX2 0.4.1 does on G52.
"""
import pathlib
import re
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <path/to/ek2.smali>", file=sys.stderr)
        return 2
    path = pathlib.Path(sys.argv[1])
    text = path.read_text()
    idx = text.find("setMaliSX2Offered")
    if idx < 0:
        print("error: setMaliSX2Offered call not found", file=sys.stderr)
        return 1
    prefix = text[:idx]
    pattern = re.compile(
        r"invoke-virtual \{v1\}, Ljava/lang/Boolean;->booleanValue\(\)Z"
        r"(?:(?!move-result v1).)*move-result v1",
        re.S,
    )
    matches = list(pattern.finditer(prefix))
    if not matches:
        print("error: Boolean.booleanValue()/move-result block not found", file=sys.stderr)
        return 1
    m = matches[-1]
    prefix = prefix[: m.start()] + "const/4 v1, 0x1" + prefix[m.end() :]
    path.write_text(prefix + text[idx:])
    tail = (prefix + text[idx:])[max(0, idx - 300) : idx + 120]
    if "const/4 v1, 0x1" not in tail:
        print("error: patch did not land next to setMaliSX2Offered", file=sys.stderr)
        return 1
    print(f"patched {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
