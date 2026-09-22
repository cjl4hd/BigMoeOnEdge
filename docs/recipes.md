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
- Future sections: one per measured model, same shape (LFM2.5 next).

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
