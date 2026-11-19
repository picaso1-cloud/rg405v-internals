# 18 — SD card validation (fake "256GB" + genuine Samsung replacement)

**2026-11-19 — replacement card: GENUINE ✅** (quick-gate results; full
certification running)

Samsung `EE8QT` (manfid 0x1b / oemid `SM`, made 11/2019), bought as 256 GB,
**pre-owned — ex-drone** (DJI `MISC/LOG/gimbal_log.log` artifacts on it):

- Capacity **238.5 GiB** = correct decimal 256 GB (vs the fake's impossible
  250 GiB), UHS-I **SDR104** on the laptop's Realtek reader (proves the old
  card's 24 MB/s was the card, not the reader).
- `f3probe`: *"Good news: the device is the real thing"* — usable = announced,
  no fakery (45 s probe).
- Read sweep 0→224 GiB: **86.6–92.1 MB/s**, flat. Far-end 1 GiB write+verify
  (the test that condemned the fake): **PASS**.
- Full f3write/f3read certification: started, result pending.

## Fake card (2026-11-11, laptop SD reader)

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

## Salvage (2026-11-11 evening): capped to 47 GiB — pending certification

Rather than discard the card, the established fake-flash salvage protocol was
applied (f3 8.0, built from source; `parted` installed for f3fix):

1. `f3probe --destructive` (0.6 s): **counterfeit, type "limbo"; usable
   52.39 GiB** of announced 250 GiB (matches the manual bisect: 48 ✅ / 52 ❌).
2. `f3fix --last-sec=98567167` → single **47 GiB partition** (`RGB10ROMS`,
   FAT32), capped 1 GiB below the last verified-good large write, leaving the
   fake zone unallocated forever.
3. **Real-file wall demo** (user's hypothesis "works above 120 GB" — tested
   with real hub ROMs, written raw outside the partition):
   - *Worms Armageddon (PSX)* → 52 GiB: **wrong immediately, data gone**
   - *Sonic Adventure (DC)* → 120 GiB: **wrong immediately, data gone**
   - Neither stored even warm — writes above the true capacity are discarded
     or mis-mapped instantly; no power cycle needed to lose them.
4. Full `f3write`/`f3read` certification of the 47 GiB partition: **✅ PASS**
   — `Data OK: 46.99 GB (98,540,928 sectors); Corrupted 0; Lost 0; Changed 0;
   Overwritten 0` (write 13.14 MB/s, read 22.74 MB/s).
5. **Real-files ground test: ✅ PASS.** 27,686 real ROM files (~27 GiB: neogeo,
   fbneo, mame2003, cps2, megadrive, nes, gbc, mastersystem, gamegear,
   pcengine, snes, n64, psx, gba, dreamcast — whole + partial selections, 58 %
   of the partition) copied from the Transcend hub → SHA-256 manifest →
   **real power cycle** (`modprobe -r rtsx_pci_sdmmc`, the laptop's Realtek
   reader) → cold re-read → **all 27,686 files identical**.

**FINAL VERDICT: certified stable at 47 GiB.** Deployed target: RGB10 Max 2
ROM card (replacing an 8 GB card). Usage rules: treat
as 47 GiB forever (never format the "unallocated" rest in Windows), read-mostly,
≤90 % fill, hub stays master copy, re-run `f3read` occasionally.
**Firmware-flash warning:** handheld CFW (ArkOS/ROCKNIX etc.) auto-resizes the
data partition to fill the *reported* card on first boot — on this card that
would grow into the dead zone. After flashing, disable the resize script and
verify the last partition ends below 47 GiB before first boot.

## Why controller reflashing is not an option (researched 2026-11-19)

The famous "reflash fake flash with an MPTool" fix is **USB-stick-only**:
USB controllers expose a vendor channel that factory Mass Production Tools
(Phison MPALL, AlcorMP, SMI MPTool, FirstChip FCMpTool…) can drive, after
identifying the chip via ChipGenius VID/PID. **SD/microSD cards have no
equivalent**: the package is monolithic (NAND + controller molded together),
the SD command set exposes no firmware-update channel, and no public MPTool
exists for no-name SD controllers. RMPrepUSB's author: *"there are no 'repair'
or reprogramming tools available for SD cards."* Gillware (recovery lab):
firmware-repair tools for SD cards target legitimate firmware; applied to
tampered firmware they typically brick the card. A successful reflash would
also only make the card report ~52 GiB truthfully — which the f3fix partition
cap already achieves in practice. **Conclusion: the capped card is the best
possible state; do not attempt firmware "repair" tools on it.**

**Method note:** f3probe is probabilistic — the f3 author recommends the full
f3write/f3read pass after any f3fix, which is exactly step 4. Tools kept at
`/tmp/opencode/f3-8.0/` (rebuildable: `make` + `make extra`).

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
