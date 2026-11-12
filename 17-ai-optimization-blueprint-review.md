# AI optimization layer — review of the "autonomous Android" blueprint

**Status:** design review + on-device bench, 2026-11-12 · **no system changes**
**Subject:** Cactus Needle 3 + Magisk + shadow mode + closed-loop telemetry
**Verdict:** the **telemetry + shadow-mode discipline is right**; the
**"autonomous root AI" framing is wrong for this device**. Keep ~20 %, reframe
~40 %, drop ~40 %. Build the deterministic layer first, use AI **offline** on
the logs, and treat the on-device model as a narrow, assist-only router.

---

## 1. One-paragraph verdict

Needle 3 is real and it **runs on the RG405V** — we measured it (engine 4.1.0,
20-layer weights, 5-tool schema): **2.0–2.4 s** per one-shot call, **~76 MB**
peak RAM, ~110 tok/s decode, **8/10** curated prompts routed correctly. But it
is a **grammar-constrained tool router**, not a planner: at **8 layers it
misroutes most prompts**, its confidence on correct calls ranges **0.38–0.88**
(so a 0.85 gate would execute 1/10), and its one dangerous answer — *"battery
saver for the train" → `max`* — was only stopped by the engine's own
suppression. The device has **0 wake locks, 0–8 mA idle, no GApps**, and the
only real knobs are three GammaOS performance profiles, four fan modes and a
few emulator settings. That is a **lookup table**, not a reasoning problem. The
useful parts are the **journal, the shadow mode and the correlation** — and
those are already half-built in doc 16.

Scorecard for the blueprint as written:

| Area | Verdict |
|---|---|
| Shadow mode / dry-run before live | **Keep** — best idea in the document |
| Telemetry correlation with the watchdog | **Keep** — already possible today |
| Needle as a bounded intent router | **Reframe** — assist-only, ≤5 tools, no root |
| Boot-level resident AI daemon | **Drop** — on-demand CLI, not a daemon |
| Autonomous governor/fan changes | **Reframe** — deterministic table, not AI |
| Dynamic zRAM resizing | **Drop** — risky, no upside |
| Cgroup freezing of background apps | **Drop** — already enabled (`use_freezer=true`) |
| Automated UAD-ng debloating | **Drop** — dangerous, nothing left to remove |
| FSR / upscaler routing | **Drop** — not exposed by these emulators |
| AI box-art/ROM renaming | **Reframe** — deterministic DAT first, AI only for fuzzy review |
| 7-day live shadow only | **Reframe** — replay the logs offline; cheaper and faster |

---

## 2. Fact check: what Needle 3 actually is

Verified from the model card, the project page, the engine manifest and the
binary itself (2026-11-12):

| Claim in the blueprint | Reality | Impact |
|---|---|---|
| "8–29 MB, 20-layer ≈ 29 MB" | Correct family; the **shipped 20 L file is 34 MB** (121 M params, CQ2 ≈ 2.125 bit) | Fits easily |
| "Simple Attention Network" | Correct — *Laddered* SAN: Monarch-Hadamard MLP, GQA + causal conv taps, hashed n-gram **engram** (70.8 M of 121 M params) | Real, not marketing fluff |
| "Official ARM64 binary" | Correct — `cactus` is an **Android NDK arm64 PIE**, interpreter `/system/bin/linker64`, engine 4.1.0 | Confirmed running |
| "Runs on a Pi Zero 2W" | **Not verified.** Official numbers are **Pi 5**: 400–4 k tok/s. On the T618 we measured ~110 tok/s decode | Re-benchmark, don't quote |
| "Minimal CPU/GPU overhead" | True per token, but it is **CPU inference**; ours used ~76 MB and 2 s per call | Pre-launch only |
| "Acts as an intelligent router" | Correct — this is exactly its strength | The one honest use |
| "Reasoning" | A short grounding trace, **not** reasoning. Author: "not true reasoning … can become nonsensical when wrong" | Do not trust the trace |
| "Decision-making brain" | It cannot plan, count reliably, or handle implied references | Wrong framing |
| "No post-training quantization needed" | True for the shipped CQ2 weights; **fine-tuning is still required** for your tool set | Budget a dataset |
| "Confidence scores gate execution" | Real, calibrated head with an engine floor of 0.1 — but it scored **0.38–0.88 on correct calls** here | 0.85 gate is unusable |

What the vendor itself says to do:

- **No tool retrieval head** — it reads the whole toolset; keep it small.
- One narrow tool per action, plain descriptions, enums that match user wording.
- `triggers` (case-insensitive regex) for intents that must always hit a tool.
- Below the confidence floor, calls move to `suppressed_calls`.
- It **refuses** when no tool fits — that is a feature, not a failure.

Community evidence (Hacker News launch thread, 54 days old, anecdotal but
consistent): confident misroutes ("my car crashed" → `play_music`), unit
conversion failures, degradation past ~10 tools, broken follow-ups. One user's
fine-tuned benchmark: 32 % correct tool shape / 20 % exact args vs 91 % / 85 %
for a FunctionGemma fine-tune. **Zero-shot is not production-grade.**

---

## 3. Measured on the RG405V (2026-11-12)

Harness and raw runs: `tools/cactus-bench/`. Pinned:

| Artifact | SHA-256 |
|---|---|
| `cactus` android-arm64 4.1.0 | `6279b8c23ed5bebb36b9dd93e485e4fad629b20785167c7ae558975533e6922f` |
| `needle3.cact` 20 L | `c9d915eca282ed42d1a09b143b592adb4cc6744ffe2d294adf5cfc5548170c38` |

Environment: T618, 4 GB, Android 14 GSI, `stock` profile, 78 % battery,
`--depth 20 --threads 2` unless stated.

### 3.1 Performance

| Setting | Wall time | Decode | Peak RAM |
|---|---|---|---|
| threads 1 | 3.85 s | 71 tok/s | 75.9 MB |
| **threads 2 (default)** | **2.0–2.4 s** | **~110 tok/s** | **~76 MB** |
| threads 4 | 2.3–2.9 s | ~70 tok/s | ~76 MB |
| depth 8, threads 4 | 1.1–1.3 s | ~160 tok/s | ~76 MB |

- **2 threads is optimal** on this SoC (the 2×A75 "fast cores"); the default
  already picks it. 4 threads are slower, presumably because work spills to the
  A55 cluster.
- **~76 MB peak**, not the 28 MB in the vendor sample JSON. Still acceptable
  for a pre-launch call on a 4 GB device, but it is **not** a background cost.
- One-shot load + prefill + decode is ~2 s. Fine before a game launches; too
  slow for an in-game overlay.

### 3.2 Accuracy and confidence (10 curated prompts)

| Prompt | Expected (rule table) | Router | Conf | Verdict |
|---|---|---|---|---|
| "ps2 time" | `start_session(ps2)` | same | 0.72 | pass |
| "I'm about to play Capcom vs SNK 2 on PS2" | `start_session(ps2)` | same | 0.38 | pass |
| "make it quiet" | fan `off` | fan `auto` | 0.38 | pass-ish (auto = off at idle) |
| "battery saver for the train" | `powersave` + fan `off` | **`max` (withheld)** | 0.29 | **FAIL** |
| "this n64 game is stuttering" | `max` + fan `cool` | `start_session(n64)` | 0.57 | conditional¹ |
| "keep the screen on for five minutes" | `300000` ms | `300005` ms | 0.50 | pass (5 ms off), flagged ungrounded |
| "turn the fan off" | fan `off` | `off` | **0.88** | pass |
| "set the CPU to max and the fan to cool" | 2 calls | 2 calls | 0.66 | pass |
| "what's the weather tomorrow" | refusal | refusal | 0.75 | pass |
| "play some music" | refusal | refusal | 0.60 | pass |

¹ Pass **only if** `start_session(n64)` applies `max` + `cool` in the Tier-B
table; otherwise it starts a session but never boosts.

**8/10 clear passes, 1 conditional, 1 dangerous failure** (auto-suppressed).
At a 0.85 confidence gate only **1/10** would execute — so confidence cannot be
the only gate, and lowering it to 0.35 would also admit bad calls.

### 3.3 The 8-layer result is disqualifying

At `--depth 8` the same 10 prompts produced **1/10** correct:

- "ps2 time" → `powersave`
- "I'm about to play CvS2 on PS2" → `stock`
- "this n64 game is stuttering" → `powersave`
- "play some music" → **fan `max` at 0.51 confidence** (would execute at 0.5)
- "keep the screen on for five minutes" → **500053 ms** (8.3 min)
- "what's the weather tomorrow" → a degenerate reasoning string that repeats
  `"time"` hundreds of times, then a suppressed `set_screen_timeout`

So the ladder is not a free RAM knob here: **use the full 20 layers**, or don't
ship the router.

### 3.4 Other device findings

- **`cmd game` is not a performance surface.** ARMSX2, PPSSPP and Flycast
  expose only `[standard,custom]`; RetroArch and Mupen64Plus AE are
  `unsupported`. GammaOS `persist.gammaos.performance_mode` remains the real
  knob.
- **Cached-app freezer is already on** (`use_freezer=true`, binder freeze
  enabled). The blueprint's manual cgroup freezing is redundant.
- **zram already supports `lzo lzo-rle [lz4] zstd`**, 2.3 GB, lz4 active. No
  reason to resize it live.

Test artifacts live in `/data/local/tmp/{cactus,needle3.cact,tools.json,bench.sh}`
(35 MB total, root-only; remove with `rm`). Nothing was installed at boot and
no setting was changed.

---

## 4. Device reality check

The blueprint's premise is "Android is bloated, an AI can reclaim it". The
2026-11-12 audit says otherwise:

| Signal | Measured | Consequence |
|---|---|---|
| Wake locks | **0** | nothing to fix |
| Idle drain | **0–8 mA** (60 mAh / 6 h 34 min) | nothing to fix |
| GApps / telephony / NFC | absent / not running / disabled | nothing to debloat |
| Running services | SystemUI, Daijishō, NetworkStack, BT, location, launcher, IME | all needed |
| Already-disabled apps | egg, bookmarkprovider, dreams.basic, printspooler, NFC, live-wallpaper | already done |
| zram | 2.3 GB, lz4, ~0.5 GB used | healthy; don't resize live |
| Performance knobs | `persist.gammaos.performance_mode` = `max/stock/powersave` | 3 states, already scripted |
| Fan | `off/auto/cool/max` via `persist.gammaos.fan_mode` | 4 states, already scripted |
| Display | 640×480 native | upscaling has little headroom |
| SoC/GPU | T618, 2×A75 + 6×A55, Mali-G52 MP2 | **no NPU path**, CPU-only inference |

So the "optimization" surface is: **which of 3 clock profiles, which of 4 fan
modes, which emulator config**. That is a lookup table, not a reasoning
problem. The real gains left are in **per-game profiles and thermal/fan
policy**, and those are deterministic.

Additional constraints the blueprint misses:

- **No `system` partition writes.** GammaOS is a GSI on dynamic partitions;
  "system init script" is not a thing. Magisk module or `service.d` is correct.
- **Android 14 restricts background activity starts.** A root shell cannot
  "listen for intents" cleanly. Real options: Daijishō's per-system launch
  commands (best), a foreground service with `UsageStatsManager` (battery
  cost), or a `logcat` watcher (fragile).
- **Process polling is expensive.** `dumpsys activity` every 5 s is ~17 k
  wakeups/day — worse than anything it would save.
- **zram live resize** needs `swapoff`/`mkswap`/`swapon` under memory pressure;
  it can stall or OOM the foreground app.
- **Cgroup freezing** is already done; manual freezing risks the launcher,
  Daijishō and alarms.
- **FSR** is a shader (FSR 1) that these emulators do not expose; source ports
  need per-engine integration. An AI cannot "route" it.
- **UAD-ng lists are generic** and mostly target OEM firmware, not a GSI that
  is already stripped. Auto-disabling system packages can break the launcher,
  the PS2 driver setup or boot. Hard no.

---

## 5. Item-by-item review of the blueprint

| Blueprint item | Verdict | Why |
|---|---|---|
| "Embedded AI acts as intelligent router" | **Reframe** | Only if the input is fuzzy language or a filename. Telemetry thresholds are not language. |
| 20-layer first, step down to 8 MB | **Keep (20 L only)** | Measured: 8 L misroutes 9/10. Use 20 L or drop the router. |
| SAN / CQ2 efficiency | **Keep** | 76 MB / 2 s measured; verify per firmware. |
| Magisk module at boot | **Reframe** | Install via Magisk, but **do not keep it resident**; invoke the CLI per event. |
| Intent listeners for app launch | **Reframe** | Use Daijishō launch hooks; avoid polling and background-activity hacks. |
| Dynamic CPU/GPU governor scaling | **Reframe** | Deterministic per-emulator table → `perfmode.sh`. AI only for unknown packages. |
| Launcher parameter injection | **Keep (small)** | Config swap is a file copy; AI can classify the game into a profile. |
| FSR / spatial upscaling | **Drop** | Not exposed, costs GPU, 640×480 panel. |
| Box-art scrape + AI verify/rename | **Reframe** | DAT/hash matching first; AI only proposes fuzzy matches into a review queue. |
| Dynamic zRAM sizing | **Drop** | Risky, no measured benefit. |
| Cgroup freezing during games | **Drop** | Already enabled; risk > reward. |
| Automated UAD-ng debloat | **Drop** | Dangerous; audit found nothing to remove. |
| 7-day shadow mode | **Keep, extend** | Add **offline replay**; 7 live days may yield too few events. |
| Confidence thresholds | **Keep, harden** | Necessary but insufficient; measured 0.38–0.88 on correct calls. |
| Closed-loop telemetry correlation | **Keep** | The strongest part; the watchdog already produces the data. |

---

## 6. The architecture that actually works

Three tiers, in order of value. Build top-down; do not start with the model.

### Tier A — Offline analyst (recommended now, zero device cost)

The intelligence belongs on the workstation, not the handheld.

```
watch.log + batterystats-*.csv + session journal
        │  (weekly pull over ADB)
        ▼
strong model / this agent on the PC
        │  anomalies, candidate rules, A/B plan
        ▼
human approves → edit rg405v-profiles.conf / watchdog thresholds
```

- No root, no battery cost, no prompt-injection surface on the device.
- Uses a model that can actually reason over a week of logs.
- Output is **rules**, not actions: "PS2 sessions above 78 °C correlate with
  `stock` + fan `cool`; propose `cool` at launch".
- This is the "agentic workflow" with the risk removed.

### Tier B — Deterministic session policy (build this first)

A ~100-line root script, no AI:

```sh
rg405v-session start ps2 "Capcom vs. SNK 2"
#  → perfmode max (or stock), fan cool, journal marker, baseline snapshot
rg405v-session stop
#  → perfmode stock, fan auto, summary (max temp, throttles, avg clocks)
```

- Table: `system → {profile, fan, emulator config}` in a plain conf file.
- Hook it into Daijishō's per-system launch/exit commands, or a Magisk
  `service.d` helper.
- This covers 90 % of the blueprint's "dynamic hardware scaling" with none of
  the risk, and it is auditable.

### Tier C — Tiny router (optional, assist-only)

Only after Tier B exists and the replay experiment passes.

```
user/launcher text ──► needle CLI (one-shot, --depth 20 --threads 2)
                          │ JSON: tool + args + confidence
                          ▼
                    dispatcher (root)
                    - tool name ∈ enum
                    - args typed + range-checked
                    - no shell interpolation
                    - journal before/after
                          ▼
                    fixed script (perfmode/fan/...)
```

- **5 tools max:** `set_performance_profile`, `set_fan_mode`,
  `set_screen_timeout`, `start_session`, `note` (no-op for shadow).
- Invoked **before** a game launches or on an explicit user phrase, never
  during gameplay. Fan is the only exception (thermal safety).
- If confidence is low → log and do nothing (or show a prompt). Do **not** pick
  a threshold until the replay set gives a precision/recall curve.
- The router is a convenience layer. If it is removed, Tier B still works.

### What not to build

- A resident model daemon (76 MB + idle CPU for a 2 s call).
- Any tool that takes a free-form shell string.
- Auto-debloat, live zram resize, manual cgroup freezing, FSR routing.
- Anything that writes `boot`, `super`, U-Boot or SPL (see ADR-011).

---

## 7. The experiment that settles it

The 10-prompt smoke test above is a sanity check, not a verdict. The deciding
test is a **replay over real logs**.

1. Build an event set from 7 days of `watch.log`: one event per snapshot where
   `temp > 75 °C`, `zram used > 1.2 GB`, `battery < 20 %`, or a session marker
   exists. Add 20 synthetic phrasings ("ps2 time", "make it quiet", "battery
   saver", "this n64 game stutters").
2. Run the **linux-x86_64** engine on the PC with the same 5-tool schema (or
   the Android CLI on-device with `--serve` for batch speed). Log
   `function_calls`, `confidence`, `reasoning`, latency, `peak_ram_mb`.
3. Compare with the deterministic table on the same events.
4. Metrics: appropriate-action rate, false-action rate, abstention rate,
   latency, RAM; plus a precision/recall curve by confidence.
5. **Kill criteria:** false-action rate > 2 %, or it fails to beat the table on
   ≥ 80 % of events → drop the model from the runtime path; keep Tier A.
6. Only if it passes: 7 days live shadow (no execution), then assist-only with
   confirmation, then a narrow auto mode for **fan only**.

**Why offline first:** a week of live shadow may produce a handful of real
events, and it burns device time. Replay gives thousands of decisions in
minutes, on the same weights, with no root risk.

**Live shadow log:** one JSON object per line — timestamp, input text, proposed
tool, args, confidence, the rule-table answer, and the watchdog snapshot.

---

## 8. Safety contract (non-negotiable if it ever executes)

1. **Allowlist, not blocklist.** Tool name must be in a fixed enum. Unknown →
   log + no-op.
2. **No `sh -c` with model output.** The dispatcher maps `(tool, arg)` to a
   fixed function. Never `eval`, never string interpolation.
3. **Typed, bounded args.** `profile ∈ {max,stock,powersave}`,
   `fan ∈ {off,auto,cool,max}`, `timeout ∈ 60000..600000`.
4. **Deny by default.** Missing/extra fields → reject.
5. **Rate limit.** ≤ 1 action / 30 s, ≤ 20 / hour; cooldown after any failure.
6. **Journal + rollback.** Before/after snapshot for every action; auto-revert
   on regression (temp > 85 °C, foreground app crash within 60 s).
7. **Kill switch.** `/data/adb/rg405v-assist/DISABLED` (checked every call) and
   a `stop` verb.
8. **No network.** `NEEDLE_TELEMETRY=0`, `DO_NOT_TRACK=1`,
   `CACTUS_TELEMETRY=0`; pin engine + weights by SHA-256.
9. **Untrusted input.** Sanitize text, cap length, strip control chars. ROM
   names and notifications are attacker-controlled.
10. **No boot/eMMC/SPL writes.** Ever. ADR-011 stays in force.

One more, from the measurements: **validate the arguments, not the reasoning
trace.** The five-minute prompt returned the correct `300005` ms while the trace
said "30000 ms". The grammar-constrained argument was right; the prose was
wrong. The reverse also happens, so range-check every number.

---

## 9. Corrected "master prompt" (replace the one in the blueprint)

> Build an **assist-only** optimization layer for a rooted RG405V (T618,
> Mali-G52 MP2, 4 GB, Android 14 GSI, Magisk).
>
> **Phase 1 — no AI.** Write `rg405v-session`, a shell tool driven by a plain
> `system → {performance_profile, fan_mode, config}` table. `start` applies the
> profile + fan and writes a session marker + snapshot; `stop` restores
> `stock`/`auto` and writes a summary. Hook it to Daijishō launch/exit commands.
> Add a dry-run mode and tests for the table.
>
> **Phase 2 — offline analysis.** Write a parser that merges
> `/data/adb/rg405v-watch/watch.log`, `batterystats-*.csv` and session markers
> into one timeline, and a report that lists anomalies and candidate rule
> changes. No on-device model.
>
> **Phase 3 — optional router.** Only if Phase 1/2 show a gap that rules can't
> cover: install the **android-arm64 Cactus Needle 3 CLI** (engine 4.1.0,
> 20-layer weights, `NEEDLE_TELEMETRY=0 DO_NOT_TRACK=1`) as a Magisk module,
> invoked **on demand** with `--depth 20 --threads 2`. Declare exactly five
> tools with enums and no free-form shell. Add a shadow mode that logs
> proposals + confidence to `shadow.jsonl` and never executes. Add a dispatcher
> that validates the JSON, enforces rate limits, journals before/after, and
> supports a kill switch. Do **not** implement dynamic zRAM resizing, cgroup
> freezing, UAD debloating, FSR, `cmd game` tuning, or a resident daemon.
>
> **Acceptance:** the router must beat the Phase-1 table on a replay set before
> any live execution; fan control is the only candidate for auto mode.

---

## 10. If we build it — MVP file layout

```
/data/adb/rg405v-assist/
├── DISABLED                  # kill switch
├── profiles.conf             # system -> profile/fan/config
├── rg405v-session            # Tier B (no AI)
├── assist                    # Tier C entry point (one-shot)
├── dispatcher.sh             # JSON validation + allowlist + journal
├── tools/                    # fixed scripts, no args from the model
│   ├── set-performance.sh
│   ├── set-fan.sh
│   └── set-timeout.sh
├── engine/                   # cactus CLI + needle3.cact (pinned)
├── shadow.jsonl              # proposals, never executed
└── actions.jsonl             # executed actions + before/after
```

Journal line (one JSON per line):

```json
{"ts":"2026-11-12T18:04:11Z","mode":"shadow","input":"ps2 time",
 "tool":"start_session","args":{"system":"ps2"},"confidence":0.93,
 "rule_answer":"start_session(ps2)","watch":{"temp":71,"zram_used_mb":512}}
```

---

## 11. Open questions (must be answered by measurement, not opinion)

1. Cold-start vs warm-start latency (page cache) on the T618.
2. Behaviour under sustained thermal load — every bench above was a cold call
   at `stock`, not during a PS2 session.
3. Non-English phrasing; the model supports 7 languages, but the tool
   descriptions were English-only here.
4. Whether a fine-tune on 200–500 real RG405V phrases fixes the
   "battery saver → max" class of error; if yes, the router becomes defensible.
5. Does the router beat the rule table on the real replay set? (The deciding
   test.)
6. Is there a real per-game win left, or is `max + cool` already optimal? The
   watchdog can answer this in one week of sessions.

---

## 12. Revisit triggers

- Needle ships a **planner/multi-step** mode, or tool-call accuracy on custom
  schemas becomes independently verified at > 90 %.
- Android exposes a stable, low-cost **app-launch callback** for root services.
- The device grows a real optimization surface (per-app GPU governor,
  kernel-level frame pacing) where a policy engine has more than 3 choices.
- The watchdog shows a regression that rules cannot explain.

Until then: **Tier A + Tier B only**. They are cheap, reversible, and they
produce the data that would justify Tier C.

---

## 12.1 Addendum — review of the implementation brief (2026-11-12)

A second-pass brief adopted the three-tier design. It is **~90 % aligned and
safe**, but it has one load-bearing false assumption and four under-specified
mechanisms. Fix these before writing Phase 1.

### The false assumption: Daijishō "launch and exit commands"

The documented Daijishō player model is **an Android intent plus extras**
(`am start -n <activity> -e ROM {file.path} …`) with a "kill process" flag.
There is **no documented pre/post-launch shell hook**. A root script cannot
simply "hook into Daijishō" to run before and after a game.

Options, best first:

1. **Wrapper activity in the existing `org.andr36oid.panel` app.** Daijishō
   points at the wrapper as the player; it applies the profile, forwards the
   original intent to the emulator, and restores on return/timeout. Requires a
   small APK change, but it is a real, testable integration with lifecycle
   events.
2. **Root `logcat` watcher** on `ActivityTaskManager` `START`/`Displayed`
   events. Event-driven and cheap, but string-parsing is fragile across
   Android builds.
3. **`dumpsys activity` polling** — rejected (≈17 k wakeups/day).

Verify option 1 on the device before committing to it.

### The missing input channel for Tier C

The brief never says **what text the router receives**. If Daijishō triggers
it, the system/package is already known — the model adds nothing. If the user
types or speaks, where is that UI? Define one before building the router:
a Panel quick-action with six fixed phrases, a text box, or drop Tier C for
now. Without an input channel, Tier C is a solution looking for a problem.

### Four mechanisms that are named but not designed

1. **Session metrics.** `watch.log` samples every 10 min; a 20-min session may
   yield one or two points. "Peak temp, throttles, average clocks" needs a
   session sampler (30–60 s) or `dumpsys thermalservice` history, plus a
   defined throttle signal (thermal status ≥ 2, or `scaling_max_freq` drop).
2. **Config swaps.** Emulators rewrite their config on exit. A swap while the
   app is running can be lost or corrupt the profile. Only swap with the app
   stopped, back up first, checksum, and never let a model choose a path.
3. **Crash detection / rollback.** "Foreground app crashes within 60 s" needs a
   detector (`logcat` crash buffer or `dumpsys activity`). Revert only if the
   current setting still equals the one we set (compare-and-swap), otherwise a
   user change is silently overwritten.
4. **Leash and boot recovery.** If the user kills the emulator, the session may
   never call `stop` and the device stays on `max`. Add: restore after N minutes
   without the session app in the foreground, and restore `stock`/`auto` at
   boot from a state file. Add a lock file for concurrent launches.

### Two policy gaps worth fixing now

- **Thermostat, not a static fan.** The brief sets fan once at launch. The
  winning deterministic rule is adaptive: `> 75 °C → fan max`, `> 82 °C →
  profile stock`, `> 88 °C → pause/alert`. This is more valuable than Tier C.
- **Battery-aware start.** `< 15 %` and not charging → `powersave` regardless
  of the game profile, unless the user overrides.

### Small corrections

- "Build top-down" contradicts "Tier B first"; say **B → A → C**.
- Kill criteria: **0 %** false actions for profile changes, **2 %** for fan;
  "beat the table on ≥ 80 % of events" needs a confidence curve, not one gate.
- Tier A should use a local model or redact game titles if it goes to a cloud.
- If the 34 MB weights and 1.5 MB engine are published, add Apache-2.0
  `LICENSE`/`NOTICE` and fetch by SHA-256 rather than committing binaries.
- `start_session` overlaps `set_performance_profile`/`set_fan_mode`; our test
  showed "n64 stuttering" choosing `start_session` instead of `max`. Define
  precedence or add triggers, or merge the tools.

### Revised Phase-1 acceptance

`rg405v-session` + a real launch wrapper; `profiles.conf` schema tests;
dry-run; session sampler; state file; auto-restore on exit/crash/boot; thermal
escalation; no polling. Tier C stays out until the input channel and replay
gate exist.

---

## 13. Sources

- Cactus Needle 3 model card — <https://huggingface.co/Cactus-Compute/needle3>
- Cactus engine 4.1.0 manifest / Android targets —
  <https://huggingface.co/Cactus-Compute/cactus/tree/main/releases/4.1.0>
- Cactus project page (sizes, confidence, triggers, deploy targets) —
  <https://cactuscompute.com/needle>
- Cactus repository / license —
  <https://github.com/cactus-compute/cactus>
- Hacker News launch thread (independent failure modes) —
  <https://news.ycombinator.com/item?id=49748553>
- On-device harness and raw runs: `tools/cactus-bench/`
- Device measurements: `16-power-audit-and-watchdog.md`
- Existing knobs: `tools/scripts/perfmode.sh`, doc `09-mods-and-tuning.md`
