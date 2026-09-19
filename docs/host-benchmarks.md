# Host benchmarks — features on our own models

Measured on the maintainer's host: 4-core x86-64, 11 GB RAM, SSD, Linux. Protocol:
256-token greedy generations with the fixed essay prompt (`scripts/bench-report.sh`),
unless a row says otherwise; cell (c) rows use the verified 3-turn 42/52/62 chain
(`scripts/cellc.sh`, 160-token budget). Every number is a real measurement; the exact
commit each cell was measured on is listed per row, because a benchmark without its
engine commit is an anecdote.

Scripts: `bench-report.sh` lives here on main; `cellc.sh` and the bridge
(`bmoe-serve.py`) live on the `feat/serve-bridge-arm64` / `feat/session-residency`
branches until #197 merges — the cell (c) rows were measured from the arc tree at
`238aef6`.

| Cell | What runs |
|---|---|
| **a) baseline** | Plain mmap load, no streaming (`bmoe-cli -m … -n 256`) — what any llama.cpp-based runtime does |
| **b) bmoe streaming** | `--moe-stream --cache-mb auto --io-threads 4 --overlap --dense-weights anon` (the engine's lossless stack) |
| **c) bmoe + llama-side features** | (b) served over the bridge with session residency: warmup replay, `--auto-echo` append reuse; engine built on the `bench/host-rs` llama branch (recurrent snapshot rollback fixes) with `--rs-seq` eligible |

## Ornith 1.5-35B (Qwen3.5-family MoE, 20.2 GB, ~2× host RAM)

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `238aef6` | 145 | 31.9 | **1.39** | — | — | **470** |
| b) bmoe streaming | arc `238aef6` | 86 | 23.4 | **2.06** | 47.1 MiB | 88.6% | 129 |
| c) warmup off | `bench/host-rs@2a8d47ac9` | — | 40.7 (T1) | 1.2–2.0 | — | — | — |
| c) warmup on | `bench/host-rs@2a8d47ac9` | — | **8.6 (T1, 4.7×)** | 1.7 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | **12.8 (T3, −50%)** | **2.54 (T3, +69%)** | — | — | — |

Reading the cells:

- **(a) → (b)**: streaming nearly doubles decode (+48%) while major faults collapse from
  470 to 129 per token — the mmap baseline is thrashing, the streamed run is reading.
- **(c) warmup**: first-turn prefill drops 40.7 → 8.6 s because the engine replays the
  conversation prefix while the bridge boots.
- **(c) auto-echo**: turn 3 prefills 28 tokens reusing 98 (`n_reused` 0 → 98) and decodes
  69% faster with verified answers. Turn 2 stays a full clear — the first echoed turn's
  render must reconcile with what was generated; every later turn rides the cache.
- Bridge-mode tok/s is lower than the CLI cells by design: short answers, reasoning
  headroom and per-request overhead are included there and not in a raw 256-token run.
- `--rs-seq` was left off for Ornith: it is a hybrid where snapshot rollback needs the
  upstream fixes (present in `bench/host-rs`), but Ornith's qwen35 template bakes an
  empty `<think>` span into re-rendered history, so echo-style reuse cannot engage on it
  structurally — same finding as Qwen3.5-9B (docs/serve.md).

Queued next: Cyber-Tiel-Coder-35B-A3B-MTP (qwen35moe, MTP-carrying), LFM2.5-8B-A1B,
Laguna-XS-2.1, Ling-mini-2.0, Qwen3-30B-A3B, Qwen3.6-35B-A3B, OLMoE-1B-7B.
