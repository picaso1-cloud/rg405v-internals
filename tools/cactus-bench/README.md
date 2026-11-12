# Cactus Needle 3 — RG405V bench harness

Reproducible one-shot test of the **android-arm64 engine 4.1.0** with the
**Needle 3 20-layer** weights, using a 5-tool schema for the RG405V.

## Pinned artifacts

| File | Size | SHA-256 |
|---|---|---|
| `cactus` (android-arm64, engine 4.1.0) | 1,636,152 B | `6279b8c23ed5bebb36b9dd93e485e4fad629b20785167c7ae558975533e6922f` |
| `needle3.cact` (20 L, 121 M, CQ2) | 35,335,380 B | `c9d915eca282ed42d1a09b143b592adb4cc6744ffe2d294adf5cfc5548170c38` |

Source:
`https://huggingface.co/Cactus-Compute/cactus/resolve/main/releases/4.1.0/android-arm64/cactus`
and `https://huggingface.co/Cactus-Compute/needle3/resolve/main/needle3.cact`.

## Run

```sh
adb push cactus needle3.cact tools.json bench.sh prompts.txt /data/local/tmp/
adb shell 'chmod 755 /data/local/tmp/cactus /data/local/tmp/bench.sh'
adb shell 'cd /data/local/tmp; ./bench.sh 20 2'
```

`bench.sh <depth> <threads>` prints `ms<TAB>prompt<TAB>json` per prompt.
Tested on the RG405V (T618, 4 GB, Android 14 GSI), 2026-11-12, `stock` profile.

## Measured result (depth 20, threads 2)

| Metric | Value |
|---|---|
| Wall time per one-shot call | **2.0–2.4 s** |
| Prefill | ~190 tok/s |
| Decode | ~110 tok/s |
| Peak RAM | **~76 MB** |
| Correct semantic action | **8 / 10** curated prompts |
| Confidence on correct calls | **0.38–0.88** |

Threads: 1 → 3.85 s, **2 → 2.0 s**, 4 → 2.3–2.9 s. Default is already 2
(fast cores), which is the right choice on this SoC.

Depth: **full 20 is required.** At `--depth 8` the same prompts misroute
("ps2 time" → `powersave`, "play some music" → `fan max` at 0.51 confidence,
"five minutes" → 500053 ms), even though it runs in ~1.2 s.

Notable failure at full depth: **"battery saver for the train" → `max`**
(confidence 0.29, withheld by the engine's own suppression). See
`../17-ai-optimization-blueprint-review.md` §3.

## Files

- `tools.json` — the five tools: `start_session`, `set_performance_profile`,
  `set_fan_mode`, `set_screen_timeout`, `note`.
- `prompts.txt` — the 10 test phrases.
- `bench.sh` — timing harness (`/proc/uptime`, no `%N` dependency).
- `results-depth20-threads2.tsv` — the run quoted above.
- `results-depth8-threads4.tsv` — the ladder-depth comparison.
