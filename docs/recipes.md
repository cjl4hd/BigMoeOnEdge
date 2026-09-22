# Recipes — the measured configuration per model

The feature scoreboard in [host-benchmarks.md](host-benchmarks.md) answers "which feature
wins"; this doc answers "what should I actually run". One section per model: the
recommended flag set, where each flag comes from, what it measured on that model, and the
total speedup against the plain-mmap baseline that any llama.cpp-based runtime starts
from. Every number is copied from [host-benchmarks.md](host-benchmarks.md) (host: 4-core
x86-64, 11 GB RAM, SSD — [method](benchmark-method.md)); nothing here is measured
anywhere else, so the two docs must move together.

**Provenance legend:**

- **ours** — implemented in this engine (`core/` or the serve bridge).
- **ours, in the llama.cpp seam** — our commit in the submodule. The seam carries exactly
  two open items: the expert-ready hook (the pin's single sanctioned commit, which the
  whole streaming engine hangs off) and the LFM2 node-budget reserve (#29085).
- **ours, merged upstream** — our llama.cpp contribution, now in mainline.
- **mainline** — inherited from llama.cpp unchanged.

## Contents

- [Cyber-Tiel-Coder-35B-A3B](#cyber-tiel-coder-35b-a3b-qwen35-family-moe-coder-210-gb-2-host-ram)

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
`--auto-echo`, and `--rs-seq 64` — the residency trio.

| Flag | Provenance | Measured on this model | Verdict |
|---|---|---|---|
| streaming stack (`--moe-stream --cache-mb auto --io-threads 4 --overlap --dense-weights anon`) | **ours** | **+70%** (1.29 → 2.19 tok/s), majflt/tok 616 → 72 | **On** — the base for past-RAM models |
| `--expert-substitute 0.15` | **ours** | **+47.2%** (→ 2.773 tok/s); **+161%** (0.96 → 2.49) in warm edit-turn sessions; quality-neutral: tinyMMLU 66.0 → 67.0%, HumanEval pass@1 43/50 → 43/50 | **Use-when**: thrash — the biggest single win |
| `--drop-cold-experts 0.75` | **ours** | **+32.3%** (→ 2.493) | **Use-when**: thrash (substitute is the bigger win; the two are not yet measured together) |
| `--rs-seq 64` | **ours, in the llama.cpp seam** (#29085) | divergence-turn prefill 42.6 → 19.3 s; composes with warmup — first turn 2.6 s | **On** for served use |
| warmup replay + `--auto-echo` (bridge) | **ours** (serve bridge) | follow-up prefill 42 → 10–12 s (222/265 tokens reused) | **On** for served use |
| `--mtp --draft 3` | concept **mainline**, implementation **ours** | **+5.2%** — the cell is flash-bound, so speculation can't pay | off here; **Use-when**: compute-bound |
| `--ubatch 512` | **mainline** | protocol default | **On** (protocol) |
| `--io-two-wave` · `--release-mmap` | **ours** | not yet measured on this model (+25.2% / +5.2% on Qwen3.6, the other deep-thrash cell) | next cells to run here |
| `--ngram` | **ours, merged upstream** | not measured here (+7.6% on LFM2.5) | workload-dependent |
| `--n-expert-used 6` | **ours** | +24.3% (Qwen3-30B, device) | only with the quality trade accepted |
| `--route-ahead 2` | **ours** (experimental) | — | experimental, lossy |
| `--prefetch 1` · `--predict-prefetch` | **ours**, refuted | −24% / −16% (Qwen3.6 cells) | **Keep-off** |
| `--drop-in-prefill` | **ours**, refuted | **−11.6%**, decode faults 26.6 → 158.5/tok | **Keep-off** |
| `--dense-odirect` | **ours**, superseded | −0.6% | **Keep-off** (deprecated alias of `--dense-weights`) |

**Total vs the mmap baseline: decode 1.29 → 2.77 tok/s = +115%** (streaming stack +
`--expert-substitute 0.15`), at quality-neutral cost on both gates. For served sessions
the residency trio adds: first-turn prefill 45 → 2.6 s, follow-ups to ~10 s, and
divergence turns halved. Provenance tally: of the 14 rows, 12 are fully ours,
1 is mainline (`--ubatch`) and 1 is a mainline concept with our implementation
(`--mtp`) — everything that makes this model fast here is this project's.
