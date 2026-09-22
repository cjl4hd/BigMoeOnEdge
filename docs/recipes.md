# Recipes — the measured configuration per model

The feature scoreboard in [host-benchmarks.md](host-benchmarks.md) answers "which feature
wins"; this doc answers "what should I actually run". One section per model: the
recommended flag set, where each flag comes from, what it measured on that model, and the
total speedup against the plain-mmap baseline that any llama.cpp-based runtime starts
from. Every number is copied from [host-benchmarks.md](host-benchmarks.md) (host: 4-core
x86-64, 11 GB RAM, SSD — [method](benchmark-method.md)); nothing here is measured
anywhere else, so the two docs must move together.

**Provenance legend:**

- **bmoe-main** — ships in Helldez's BigMoeOnEdge (`origin/main`): the streaming engine,
  its compute/io knobs, and the expert-ready seam commit in the submodule. This fork
  builds on that engine; these are not the fork's to claim.
- **fork (this repo)** — added by the `cjl4hd` fork: the session-residency layer
  (`--rs-seq`, the serve bridge, warmup replay, `--auto-echo`) and seam PR #29085.
- **mainline (llama.cpp)** — inherited from llama.cpp (e.g. ubatch; the ngram-mod
  drafting stack, upstream PR #19164).
- **refuted / superseded** — implemented, measured, and rejected; kept per
  [ADR-005](adr/005-keep-off-features-policy.md). Provenance for these is bmoe-main's
  unless marked fork.

## Contents

- [Cyber-Tiel-Coder-35B-A3B](#cyber-tiel-coder-35b-a3b-qwen35-family-moe-coder-210-gb-2-host-ram)
- [LFM2.5-8B-A1B](#lfm25-8b-a1b-lfm-hybrid-moe-50-gb-fits-host-ram-comfortably--the-contrast-case)

## Cyber-Tiel-Coder-35B-A3B (Qwen3.5-family MoE coder, 21.0 GB, ~2× host RAM)

A thinking coder MoE at 2× host RAM: the deepest expert-streaming thrash among the coder
cells (~18% routing miss headroom) and the MTP carrier. Measured on its Q4_K_M quant.

**Recommended recipe (single-shot / cold sessions):**

```bash
bmoe-cli -m Cyber-Tiel-Coder-35B-A3B-MTP-UD-Q4_K_M.gguf \
  --moe-stream --cache-mb auto --io-threads 4 --overlap --dense-weights anon \
  --expert-substitute 0.15
```

**Add for served / multi-turn sessions** (serve bridge + engine): warmup replay,
`--auto-echo`, and `--rs-seq 64` — the residency trio (fork work, on top of the
streaming engine).

### Per-flag table (provenance marked)

| Flag | Provenance | Measured on this model | Verdict |
|---|---|---|---|
| streaming stack (`--moe-stream --cache-mb auto --io-threads 4 --overlap --dense-weights anon`) | **bmoe-main** | **+70%** (1.29 → 2.19 tok/s), majflt/tok 616 → 72 | **On** — the base for past-RAM models |
| `--expert-substitute 0.15` | **bmoe-main** | **+47.2%** (→ 2.773 tok/s); quality-neutral: tinyMMLU 66.0 → 67.0%, HumanEval pass@1 43/50 → 43/50 | **Use-when**: thrash — the biggest single decode win |
| `--drop-cold-experts 0.75` | **bmoe-main** | **+32.3%** (→ 2.493) | **Use-when**: thrash (substitute is the bigger win; the two are not yet measured together) |
| `--mtp --draft 3` | **bmoe-main** (mainline concept) | **+5.2%** — the cell is flash-bound, so speculation can't pay | off here; **Use-when**: compute-bound |
| `--io-two-wave` · `--release-mmap` | **bmoe-main** | not yet measured on this model (+25.2% / +5.2% on Qwen3.6, the other deep-thrash cell) | next cells to run here |
| `--ubatch 512` | **mainline** | protocol default | **On** (protocol) |
| `--ngram` | **mainline** (ngram-mod, upstream PR #19164) | not measured here (+7.6% on LFM2.5) | workload-dependent |
| `--rs-seq 64` | **fork** (seam PR #29085) | divergence-turn prefill 42.6 → 19.3 s; composes with warmup — first turn 2.6 s | **On** for served use |
| warmup replay + `--auto-echo` (bridge) | **fork** (serve bridge) | follow-up prefill 42 → 10–12 s (222/265 tokens reused) | **On** for served use |
| `--n-expert-used 6` | **bmoe-main** | +24.3% (Qwen3-30B, device) | only with the quality trade accepted |
| `--route-ahead 2` | **bmoe-main** (experimental) | — | experimental, lossy |
| `--prefetch 1` · `--predict-prefetch` · `--drop-in-prefill` · `--dense-odirect` | **bmoe-main**, refuted/superseded | −24% / −16% / −11.6% / −0.6% | **Keep-off** |

### Totals vs the plain-mmap baseline (two metrics)

**1. bmoe vs llama (the engine vs any stock llama.cpp-based runtime) — decode:**
streaming stack + `--expert-substitute 0.15`:
**1.29 → 2.77 tok/s = +115%**, at quality-neutral cost on both gates
(tinyMMLU 66.0 → 67.0%, HumanEval pass@1 43/50 → 43/50).
All mechanisms here are **bmoe-main's** — it is the engine's headline, not the fork's.

**2. fork vs bmoe-main (what this fork adds on top) — session/turn latency, not decode:**
the residency trio changes prefill and turn latency: first-turn prefill ~45 → 2.6 s
(warmup + `--rs-seq` composition), follow-up prefill 42 → 10–12 s (`--auto-echo`),
divergence-turn prefill 42.6 → 19.3 s (`--rs-seq`). Measured as **2.2× faster
divergence-turn prefill** and follow-ups at **~4× lower prefill**; the fork's decode
contribution on this model is 0% — its decode A/B is deliberately not run, because its
value is session latency. (The gate harness's warm-session 0.96 → 2.49 tok/s difference
is substitution's warm-regime win, a bmoe-main mechanism, not a fork-vs-bmoe number.)

Provenance tally: of the recipe's active flags, decode speed is entirely bmoe-main;
the fork contributes the session-residency layer that owns prefill/turn latency.

## LFM2.5-8B-A1B (LFM hybrid MoE, 5.0 GB, fits host RAM comfortably — the contrast case)

The fits-RAM counterpoint to the 35B cells: no thrash to eliminate (0 faults in every
cell), so the streaming engine has nothing to fix — and the session layer is the whole
show. Same 3-turn 42/52/62 protocol as Cyber-Tiel; thinking model, LFM hybrid arch
(the family whose recurrent-rollback node budget seam PR #29085 fixes).

**Recommended recipe (fits RAM — no streaming):**

```bash
bmoe-cli -m LFM2.5-8B-A1B-UD-Q4_K_M.gguf --ubatch 512
```

**Add for served / multi-turn sessions** (serve bridge + engine): warmup replay,
`--auto-echo`, `--rs-seq 64` — here they are the primary value, not an add-on.

### Per-flag table (provenance marked)

| Flag | Provenance | Measured on this model | Verdict |
|---|---|---|---|
| streaming stack (b) | **bmoe-main** | **−10.8%** (9.69 → 8.64 tok/s), prefill 1.65 → 7.9 s — no thrash to eliminate, so the stack is pure overhead | **Off** for fits-RAM models |
| `--ubatch 512` | **mainline** | protocol default | **On** (protocol) |
| `--ngram` | **mainline** (ngram-mod, upstream PR #19164) | **+7.6%** (7.566 → 8.143), 48.5% acceptance — composes with the rs rollback planes | **Use-when**: repetitive workload |
| warmup replay (bridge) | **fork** | T1 prefill **7.59 → 1.12 s (6.8×)** — pure prefill speed, no fault storm to fight | **On** for served use |
| `--auto-echo` (bridge) | **fork** | template natively echoes reasoning: reuse engages from the first follow-up (T2 24/124, T3 24/205), prefill ~1.2 s — no reconcile turn | **On** for served use |
| `--rs-seq 64` | **fork** (seam PR #29085) | divergence prefill **2.82 → 1.25 s (2.3×)**; composes with warmup — T1 at 0.12 s | **On** for served use |
| `--mtp` | **bmoe-main** (mainline concept) | not measured on this model (acceptance was measured via ngram verify) | candidate cell |
| `--expert-substitute` · `--drop-cold-experts` | **bmoe-main** | not measured — a fits-RAM model has ~0 miss headroom to trade | not applicable |

### Totals (two metrics)

**1. bmoe vs llama — negative here, and that is the finding:** with no streaming and no
faults, bmoe-cli on a fits-RAM model tracks the stock runtime (streaming stack −10.8%;
do not enable it). The engine's wins are memory-pressure wins, and this model has none.

**2. fork vs bmoe-main — the fork's best case:** session layer only, and it dominates:
first-turn prefill **6.8× faster** (7.59 → 1.12 s), follow-up prefill **~6× lower**
(7.56 → ~1.2 s), divergence prefill **2.3× faster** (2.82 → 1.25 s), plus `--ngram`
(mainline machinery) adding +7.6% decode on repetitive workloads. Decode is otherwise
unchanged — by design.

**Contrast with Cyber-Tiel, the pattern the two recipes establish:** past-RAM models
buy bmoe-main's streaming + substitution (+115% decode); fits-RAM models should leave
the stack off and take the fork's session layer (6.8×/6×/2.3× prefill and turn
latency). Model size decides which layer pays.
