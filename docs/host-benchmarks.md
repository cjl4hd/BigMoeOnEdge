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

**Summary.** Three models measured end to end across five cells each: two 35B-class MoE
hybrids ~2× host RAM (Qwen3.5-family: Ornith 1.5, Cyber-Tiel-Coder) and one 8B hybrid that
fits RAM comfortably (LFM2.5). Where the model is past RAM, streaming (a → b) roughly
doubles decode (1.39 → 2.06 and 1.29 → 2.19 tok/s) while major faults collapse ~4–8×
(470 → 129 and 616 → 72 per token): the mmap baseline thrashes, the streamed run reads.
Where the model fits, streaming is a small net loss (9.69 → 8.64 tok/s, 0 faults both
ways) — the (b) stack is a tool for memory pressure, not a default. Session residency (c
rows) removes most of the per-turn prefill everywhere: append reuse with `--auto-echo`
reuses 98–265 tokens per follow-up turn on the big models and 124–205 on LFM2.5 (prefill
to ~1.2 s), and snapshot rollback (`--rs-seq 64`) turns the worst case — a divergence the
client causes by not echoing reasoning back — from a full clear into a bounded rewind on
every model measured: 33/23 (Ornith), 33/203 (Cyber-Tiel), 26/22 (LFM2.5), answers
verified, no degeneration, with divergence-turn prefill halving on Cyber-Tiel (42.6 →
19.3 s) and dropping 2.3× on LFM2.5 (2.82 → 1.25 s).

**Conclusions.**

- Major faults per token is the most predictive number in the matrix: within every model,
  each cell's tok/s tracks its majflt/tok (616 → 1.29 vs 72 → 2.19; 470 → 1.39 vs 129 →
  2.06). A run that thrashes is slow, whatever else is true — and LFM2.5 (0 faults in
  both cells) shows the same law from the other side: nothing to fix, nothing gained,
  streaming only pays its overhead.
- The rewind's payoff depends on where the time goes. On this IO-bound host, Ornith's c5
  win was mechanism, not wall-clock (prefill 15.5 ≈ 14.5 s — cached-token compute is not
  the bottleneck); the same rewind on the coder cut prefill 2.2× (42.6 → 19.3 s) and on
  LFM2.5 2.3× (2.82 → 1.25 s) because there the skipped tokens were worth skipping. Both
  regimes are the same mechanism at the same budget.
- Warmup replay and `--rs-seq` are complementary, not redundant. The replay seeds the
  prefix (cold-start); the snapshots make its work *reusable* across divergent turns —
  c5's first turn decodes after restoring the replayed history (1 fresh token: 203
  reused on Cyber-Tiel, 22 on LFM2.5; 2.6 s and 0.12 s where the same turn otherwise
  prefills ~45 s and ~7.6 s). Without `--rs-seq`, a thinking hybrid cannot reuse the
  replay at all: the engine full-clears any non-append turn by design (verified in
  session.cpp; the warmup cells' `n_reused` 0 rows are that designed worst case, not a
  failure).
- Template capability decides how much the echo mechanism can do. LFM2.5's template
  natively supports echoed reasoning, so `--auto-echo` reuse engages from the first
  follow-up turn; the qwen35 family pays one reconcile turn first (T2 stays a full
  clear, T3+ rides the cache), and echo-style reuse never engages on it without the
  bridge — only the snapshot rewind does.
- Every reuse zero in the matrix is either a designed full clear or a structural
  template/echo mismatch — never a wrong answer. All cell answers are verified (42/52/62;
  a fast-but-wrong run reports FAIL).

**Recommendations for use.**

- Above-RAM models, raw generation: the (b) stack — `--moe-stream --cache-mb auto
  --io-threads 4 --overlap --dense-weights anon`. Lossless, measured here at +48–70%
  decode over mmap with 4–8× fewer major faults.
- Models that fit RAM: plain mmap, no streaming flags. Measured at +12% decode over the
  streaming stack on LFM2.5 (9.69 vs 8.64 tok/s) with ~5× faster prefill — turning on
  streaming for a resident model is pure overhead.
- Agent / OpenAI-client serving: run the bridge with `--auto-echo` and default warmup on.
  Expect follow-up-turn prefill to collapse to tens of tokens (from the first follow-up
  on LFM2.5-class templates; after one reconcile turn on the qwen35 family). Divergence-
  shaped turns (aider edit turns, clients that drop reasoning) still full-clear unless
  `--rs-seq` is on.
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

## LFM2.5-8B-A1B (LFM hybrid MoE, 5.0 GB, fits host RAM comfortably — the contrast case)

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `14cdfe8` | 10.5 | 1.65 | **9.69** | — | — | 0 |
| b) bmoe streaming | arc `14cdfe8` | 3.2 | 7.9 | 8.64 | 4 MiB | 97.2% | 0 |
| c) warmup off | `bench/host-rs@2a8d47ac9` | — | 7.59 (T1) | 7.2–9.9 | — | — | — |
| c) warmup on | `bench/host-rs@2a8d47ac9` | — | **1.12 (T1, 6.8×)** | 8.2–9.8 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | **1.23 (T3)** | 9.3 (T3) | — | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 7.56 / 2.82 (T2) | 7.5 / 9.1 | — | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 0.12 / **1.25 (T2)** | 8.0 / 9.4 | **T1: 1 prompt / 22 reused; T2: 26 prompt / 22 reused** | — | — |

Reading the cells:

- **(a) → (b)**: when the model fits RAM comfortably, streaming is a small net LOSS
  (9.69 → 8.64 tok/s, prefill 1.65 → 7.9 s): there is no thrash to eliminate (0 faults
  both ways), so the streamer only adds overhead. Use the (b) stack when the model is
  at or past RAM, not as a default.
- **(c) warmup**: first-turn prefill 7.59 → 1.12 s (6.8×) — on a model that fits, the
  replay is pure prefill speed, no fault storm to fight. Reuse stays 0 by the same
  designed mechanism as on the 35Bs: the first plain turn is a *prefix* of the replayed
  render, and without `--rs-seq` a thinking hybrid's later turns cannot match the
  reasoning-bearing resident render.
- **(c) auto-echo**: LFM2.5's template natively supports echoed reasoning, so reuse
  engages from the first follow-up: T2 prefills 24 tokens reusing 124, T3 24/205,
  prefill ~1.2 s — no "first echoed turn reconciles" cost like the qwen35 family.
- **(c4) → (c5) divergence**: full clear 48/0 with rs-seq off; with `--rs-seq 64` the
  rewind restores to just after the first answer — 26 prompt / 22 reused, prefill
  2.82 → 1.25 s, answer 62 verified. Second arch family (after qwen35moe) with a live
  end-to-end rollback proof, and warmup+rs-seq composition replicated: T1 comes back
  **1 prompt / 22 reused** at 0.12 s.
- LFM2.5's snapshot planes are small (22-token history rewound within the 64 budget);
  the plane-cost OOM law that binds 35B budgets does not bite at this size.

Queued next: Laguna-XS-2.1, Ling-mini-2.0, Qwen3-30B-A3B, Qwen3.6-35B-A3B, OLMoE-1B-7B.
