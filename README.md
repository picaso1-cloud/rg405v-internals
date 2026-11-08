# Anbernic RG405V — Internals (Android)

Knowledge base for the **Anbernic RG405V** (Unisoc T618, Android 12 stock).
Part of the three-device plan in `../MASTER-PLAN.md`; shared techniques in
`../SHARED-KNOWLEDGE.md`.

Evidence tags: **[seen]** observed here · **[inferred]** public docs/reasoning ·
**[unknown]** not probed.

## Device identity

| Item | Value | Evidence |
|---|---|---|
| SoC | Unisoc Tiger **T618** (2× Cortex-A75 @2.0 GHz + 6× A55) | [inferred] anbernic.com |
| GPU | Mali-G52 MP2 @850 MHz | [inferred] anbernic.com |
| RAM | 4 GB LPDDR4X @1866 MHz | [inferred] anbernic.com |
| Storage | 128 GB eMMC + microSD slot | [inferred] anbernic.com |
| Display | 4″ IPS touch, 640×480 | [inferred] anbernic.com |
| Battery | 5500 mAh | [inferred] anbernic.com |
| Connectivity | Wi-Fi 2.4/5 GHz 802.11ac, BT 5.0 | [inferred] anbernic.com |
| Stock OS | **Android 12** | [inferred] anbernic.com |
| Custom OS | **GammaOS Next v1.1** for RG405M/V/RG505 (Android 13/14) | [inferred] GammaOSNext repo |
| Linux | No community/mainline T618 handheld port known | [inferred] |

> ⚠️ Anbernic's own product page warns that installing a third-party OS may make
> it impossible to flash back to the official OS. **Back up every partition and
> prove a rollback before the first write.** [inferred]

## Goals

1. **Highest feasible Android** — GammaOS Next (13/14) with a verified rollback.
2. **andr36oid layer parity** — performance floors, thermal control, zram,
   debloat, launcher, wireless ADB, USB gadget modes, input remaps,
   Tailscale + Moonlight.
3. **Best emulation** — AetherSX2/ARMSX2 (PS2), Dolphin (GC/Wii), PPSSPP,
   Yabasanshiro, Winlator/box64 where useful — measured per title.
4. **Documentation** — full hardware/firmware profile before any modification.

## Status

- [inferred] Public specs collected (this file).
- [ ] Device probed (ADB/root/bootloader/partitions/sysfs).
- [ ] Full partition backup + rollback proven.
- [ ] GammaOS Next installed and validated.
- [ ] andr36oid layer ported.
- [ ] Emulation/perf baseline recorded.

## Docs

| File | Content |
|---|---|
| [01-recon-plan.md](01-recon-plan.md) | Step-by-step probing + backup/rollback checklist |
| `02-hardware.md` | (to write after probing) SoC/GPU/panel/input/power |
| `03-firmware.md` | (to write) stock + GammaOS Next structure, flashing |
| `04-optimization.md` | (to write) andr36oid layer port |

## First contact checklist

1. Do **not** flash anything yet.
2. Enable developer options + USB debugging; record `getprop`.
3. Dump partition table and every partition you can read.
4. Verify the exact model/firmware (`ro.product.model`, `ro.build.display.id`).
5. Confirm bootloader unlock state and the correct flash tool for T618.
6. Then decide GammaOS Next and the rollback plan.
