# Host benchmarks — features on our own models

Measured on the maintainer's host: 4-core x86-64, 11 GB RAM, SSD, Linux. Protocol:
256-token greedy generations with the fixed essay prompt (`scripts/bench-report.sh`),
unless a row says otherwise; cell (c) rows use the verified 3-turn 42/52/62 chain
(`scripts/cellc.sh`; 160-token budget, 192 on thinking models); divergence rows (c4/c5)
use the two-turn shape
where the second request resends T1 plus the engine's own plain reply and asks a NEW
question, so the hybrid must rewind its recurrent state to just after the first answer
to reuse anything. Every number is a real measurement; the exact
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

## Summary, conclusions, recommendations

**Summary.** Two 35B-class MoE hybrids (~2× host RAM, Qwen3.5-family) measured end to end
across five cells each. Streaming (a → b) roughly doubles decode on both (1.39 → 2.06 and
1.29 → 2.19 tok/s) while major faults collapse ~4–8× (470 → 129 and 616 → 72 per token):
the mmap baseline thrashes, the streamed run reads. Session residency (c rows) removes most
of the *per-turn* prefill: append reuse with `--auto-echo` reuses 98–265 tokens per
follow-up turn (prefill 42 → 10–12 s on the coder model), and snapshot rollback
(`--rs-seq 64`) turns the worst case — a divergence the client causes by not echoing
reasoning back — from a full clear into a bounded rewind: 33 prompt / 23 reused (Ornith)
and 33 prompt / 203 reused (Cyber-Tiel), answers verified, no degeneration, with the
divergence turn's prefill halving on the second model (42.6 → 19.3 s).

**Conclusions.**

- Major faults per token is the most predictive number in the matrix: within every model,
  each cell's tok/s tracks its majflt/tok (616 → 1.29 vs 72 → 2.19; 470 → 1.39 vs 129 →
  2.06). A run that thrashes is slow, whatever else is true.
- The rewind's payoff depends on where the time goes. On this IO-bound host, Ornith's c5
  win was mechanism, not wall-clock (prefill 15.5 ≈ 14.5 s — cached-token compute is not
  the bottleneck); the same rewind on the coder cut prefill 2.2× (42.6 → 19.3 s) because
  203 tokens were worth skipping. Both are the same mechanism at the same budget.
- Warmup replay and `--rs-seq` are complementary, not redundant. The replay seeds the
  prefix (cold-start); the snapshots make its work *reusable* across divergent turns —
  c5's first turn decodes after restoring 203 tokens and prefills 1 (2.6 s vs ~45 s
  everywhere else). Without `--rs-seq`, a thinking hybrid cannot reuse the replay at all:
  the engine full-clears any non-append turn by design (verified in session.cpp; the
  warmup cells' `n_reused` 0 rows are that designed worst case, not a failure).
- Every reuse zero in the matrix is either a designed full clear or a structural
  template/echo mismatch — never a wrong answer. All cell answers are verified (42/52/62;
  a fast-but-wrong run reports FAIL).

**Recommendations for use.**

- Above-RAM models, raw generation: the (b) stack — `--moe-stream --cache-mb auto
  --io-threads 4 --overlap --dense-weights anon`. Lossless, measured here at +48–70%
  decode over mmap with 4–8× fewer major faults.
- Agent / OpenAI-client serving: run the bridge with `--auto-echo` and default warmup on.
  Expect follow-up-turn prefill to collapse to tens of tokens. Divergence-shaped turns
  (aider edit turns, clients that drop reasoning) still full-clear unless `--rs-seq` is on.
- Thinking hybrids with clients that do not echo reasoning: enable `--rs-seq 64`
  (experimental, off by default). Budget by rewind depth, not context length: 64 planes
  ≈ 3.9 GiB on a 35B — larger budgets OOM a small host without buying anything, since a
  depth beyond budget refuses honestly and full-clears anyway.
- Do not expect streaming to speed up *prefill* on MoE: prefill routes nearly all experts,
  so there is nothing to skip (cell b prefill 68.3 s vs a's 61.8 s on the coder). The
  streaming win is decode throughput and stability.

## Ornith 1.5-35B (Qwen3.5-family MoE, 20.2 GB, ~2× host RAM)

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `238aef6` | 145 | 31.9 | **1.39** | — | — | **470** |
| b) bmoe streaming | arc `238aef6` | 86 | 23.4 | **2.06** | 47.1 MiB | 88.6% | 129 |
| c) warmup off | `bench/host-rs@2a8d47ac9` | — | 40.7 (T1) | 1.2–2.0 | — | — | — |
| c) warmup on | `bench/host-rs@2a8d47ac9` | — | **8.6 (T1, 4.7×)** | 1.7 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | **12.8 (T3, −50%)** | **2.54 (T3, +69%)** | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 16.7 / 14.5 (T2) | 1.65 / 1.62 | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 20.1 / **15.5 (T2)** | 1.15 / 1.18 | **T2: 33 prompt / 23 reused** | — | — |

Reading the cells:

- **(a) → (b)**: streaming nearly doubles decode (+48%) while major faults collapse from
  470 to 129 per token — the mmap baseline is thrashing, the streamed run is reading.
- **(c) warmup**: first-turn prefill drops 40.7 → 8.6 s because the engine replays the
  conversation prefix while the bridge boots.
- **(c) auto-echo**: turn 3 prefills 28 tokens reusing 98 (`n_reused` 0 → 98) and decodes
  69% faster with verified answers. Turn 2 stays a full clear — the first echoed turn's
  render must reconcile with what was generated; every later turn rides the cache.
- **(c4) → (c5) divergence**: with rs-seq off, the divergence turn is a full clear
  (56 prompt tokens, 0 reused). With `--rs-seq 64` the engine restores the recurrent
  state to just after the first answer and prefills only the plain-rendered reply plus
  the new question — **33 prompt tokens, 23 reused**, answer 62 correct with no
  degeneration. This is the first live end-to-end proof of the upstream snapshot
  rollback fix (`fix/rs-rollback-index-shift`) through the real engine on a 35B hybrid;
  the c5 budget is 64, not MAXTOK — 160 planes reserve ~10 GiB on this host and the
  kernel OOM-killer ends the run.
- Bridge-mode tok/s is lower than the CLI cells by design: short answers, reasoning
  headroom and per-request overhead are included there and not in a raw 256-token run.
- `--rs-seq` echo-style reuse cannot engage on Ornith: its qwen35 template bakes an
  empty `<think>` span into re-rendered history (same finding as Qwen3.5-9B,
  docs/serve.md) — but the **divergence rows show the snapshot-rollback path does
  engage** (c5 rewound and reused 23 tokens), so the earlier "reuse cannot engage on it
  structurally" claim holds only for the echo mechanism, not for rewind.

## Cyber-Tiel-Coder-35B-A3B (Qwen3.5-family MoE coder, 21.0 GB, ~2× host RAM; the MTP carrier for a future `--mtp` host row)

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `2070368` | 47 | 61.8 | **1.29** | — | — | **616** |
| b) bmoe streaming | arc `2070368` | 178 | 68.3 | **2.19** | 68.0 MiB | 81.9% | 72 |
| c) warmup off / on | `bench/host-rs@2a8d47ac9` | — | 45.6 / 42.4 (T1) | 1.2 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | **11.6 (T3)** | **1.6 (T3)** | — | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 47.6 / 42.6 (T2) | 1.2 / 1.3 | — | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 2.6 / **19.3 (T2)** | 1.0 / 1.1 | **T1: 1 prompt / 203 reused; T2: 33 prompt / 203 reused** | — | — |

Reading the cells:

- **(a) → (b)**: streaming nearly doubles decode (+70%) while major faults collapse from
  616 to 72 per token — the same thrash-vs-reading contrast as Ornith, wider here (a coder
  model routes more distinct experts per token).
- **(c) warmup**: T1 prefill improves only 45.6 → 42.4 s. The replay makes the prefix
  resident, but Cyber-Tiel thinks, so the resident render (reasoning included) diverges from
  the first plain-rendered turn — and on a hybrid without `--rs-seq` any non-append turn
  full-clears by design, so the replay cannot be *reused* this way. What survives is the
  cold-start part: flash reads come warm, and c4's later identical T1 shows 47.6 s without
  that warmth. Warmup *reuse* needs the snapshot pool (next rows), not the replay alone.
- **(c) auto-echo**: turn 2 prefills 28 tokens reusing 222 and turn 3 prefills 28 reusing
  265 (prefill 42 → 10–12 s), answers verified — the echo mechanism composing with a coder
  model's longer reasoning.
- **(c4) → (c5) divergence**: with rs-seq off, the divergence turn is a full clear (236
  prompt tokens, 0 reused). With `--rs-seq 64` the engine restores the recurrent state to
  just after the first answer and prefills only the plain-rendered reply plus the new
  question — **33 prompt tokens, 203 reused**, answer 62 correct, no degeneration. Unlike Ornith
  (IO-bound: the rewind proved mechanism, not latency), the skip pays wall-clock here:
  T2 prefill halves, 42.6 → 19.3 s. Also visible in c5: warmup and rs-seq **compose** —
  T1 prefill is **1 prompt / 203 reused** (the replayed history restored, 1 fresh token),
  2.6 s where every other cell prefills ~45 s.
- The c-suite on this model is fully consistent with the session-residency rules:
  every `n_reused` 0 above (warmup T1, c1–c4 turns) is a *designed* full clear or a
  structural no-echo mismatch, not a failure — with snapshots on, every divergence
  becomes a bounded rewind instead.

Queued next: LFM2.5-8B-A1B, Laguna-XS-2.1, Ling-mini-2.0, Qwen3-30B-A3B, Qwen3.6-35B-A3B, OLMoE-1B-7B.
