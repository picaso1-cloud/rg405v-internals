# ARCHIVE — parked tracks and where the evidence lives

Nothing here is deleted. This index prevents re-litigating settled questions on
the RG405V (Unisoc T618 / `ums512_1h10`).

## Parked (not closed — waiting on a dependency)

| Track | State | Evidence / resume trigger |
|---|---|---|
| **Mainline Linux port** (`rg-rotate-linux`, kernel 7.1 SPRD, `ums512_1h10`) | 📚 researched, not started | `11-linux-port-feasibility.md` §1–§3. Resume when we commit to a second card + Debian/Arch rootfs |
| **GPU acceleration / Vulkan** | 🅿️ parked 2026-11-12 | `14-gpu-acceleration-plan.md`, `15-linux-gpu-parking-and-references.md` (review triggers: PanVK milestones, MaliSX2 kbase builds, fxlin/mali) |
| **AI optimization layer** | ✅ reviewed, assist-only | `17-ai-optimization-blueprint-review.md` — deterministic-first; no autonomous root agent |
| **Multiboot menu** | 📚 design only | `13-multiboot-menu.md` |
| **TWRP / recovery** | 📚 documented | `12-recovery-and-twrp.md` |

## Superseded

| Document | Change | Note |
|---|---|---|
| `11-linux-port-feasibility.md` path A (GarlicOS 2.x as "weakest") | **Promoted to first step (2026-11-20)** | The GarlicOS *bootloader* is now understood as a chain-load from the stock verified boot chain — see `19-garlicos-bootloader-analysis.md`. Its "no HW acceleration" limit is a GPU/userspace issue, not a boot issue |

## Boundary rule (unchanged)

RG353V material lives in `rg353v-internals` / `~/rg353v-backup`. Nothing from
RK3566 experiments belongs in this repo, and nothing from T618 work belongs in
`rg353v-internals`.
