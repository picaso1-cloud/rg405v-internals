# 18 — 256 GB SD card validation (2026-11-11, laptop SD reader)

**Verdict: ❌ DEFECTIVE — do not store data on it. Return it.**

## Card identity

| Field | Value | Note |
|---|---|---|
| Product name | `asdfg` | no-name (keyboard mash) |
| Manufacturer/OEM ID | `0x05` / `0x0c` | no recognizable maker (SanDisk=0x03, Samsung=0x1b) |
| Manufactured | 12/2025 | |
| Capacity | **exactly 256 GiB** (524,288,000 × 512 B) | genuine 256 GB cards are 238.5 GiB decimal; GiB-exact sizing typical of no-name flash |
| Bus mode | **high speed** (no UHS) | ~24 MB/s ceiling on this reader; reader may also be non-UHS |
| Partition | one FAT32, full size, empty | FAT32 at 250 GiB = 4 GB max file size — would have needed reformatting anyway |

## Measured performance (laptop internal reader, "high speed" bus)

- Read: **18.6–29.2 MB/s** across beginning / 60 / 120 / 130 GB / last GB —
  consistent, no read errors anywhere.
- Write: **17.3 MB/s** (large sequential).

## The defect (reproduced, characterized)

1. **Small writes verify everywhere**: write+read-back with SHA-256 at
   32, 64, 128, 156, 187, 218, 234, 242, 243–249.9 GiB (4–32 MiB chunks) —
   **all pass**. Reads after any failure are **stable** (same wrong bytes
   every time), i.e. the controller serves consistent wrong data, not noise.
2. **Large sequential writes to the tail corrupt the whole span**: a single
   1 GiB write at the last GiB (and at 248.8 GiB, strictly inside the
   partition) returns **entire-region mismatch from byte 1** — stable wrong
   data. Small writes at the same offsets pass before and after.
   → The controller mishandles long writes in the tail region; the corruption
   window is not bounded by the failing offsets, so **data below the tail
   cannot be trusted either** (write history may relocate the failure zone).
3. Partition-boundary artifact noted: `mmcblk0p1` is 32 sectors shorter than
   the device; dd writes ending past the partition end return ENOSPC
   (expected kernel behaviour). All decisive tests were re-run strictly
   in-bounds — same failure.

## Why "cap it and use the good part" is rejected

The failing span reads back wrong-but-stable regardless of in/out-of-bounds
writes, and small writes interleave fine — the failure mode depends on write
history and size, not a fixed bad zone. A full-fill certificate of the lower
region would therefore not guarantee safety later. For a ROM library this is
a silent-corruption time bomb.

## Correct identification: how to spot this class of card

- `cat /sys/block/mmcblk0/device/{name,manfid,oemid,date}` — anonymous name,
  unknown IDs.
- Capacity exactly **power-of-two GiB** instead of decimal-GB sizing.
- Enumerates "high speed" only (never advertises UHS grades).
- Point write+verify passes everywhere but **large (>256 MiB) sequential
  writes in the last few % of capacity fail verification** — always test
  with a ≥1 GiB roundtrip near the end, not just far-end point checks.

## Method (reproducible)

```sh
# identity
cat /sys/block/mmcblk0/device/name /sys/block/mmcblk0/device/manfid
# read bench
dd if=/dev/mmcblk0p1 of=/dev/null bs=4M count=256 status=progress
# far-end roundtrip (last 1 GiB; partition ends 63999.995 × 4 MiB blocks)
dd if=/dev/urandom of=/tmp/pattern.bin bs=4M count=256 status=none
dd if=/tmp/pattern.bin of=/dev/mmcblk0p1 bs=4M seek=63700 count=256 conv=notrunc
sync
dd if=/dev/mmcblk0p1 of=/tmp/readback.bin bs=4M skip=63700 count=256 status=none
cmp /tmp/pattern.bin /tmp/readback.bin   # any output = corruption
```
