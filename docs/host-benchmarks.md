# Host benchmarks — features on our own models

Measured on the maintainer's host: 4-core x86-64, 11 GB RAM, SSD, Linux. Protocol:
256-token greedy generations with the fixed essay prompt (`scripts/bench-report.sh`),
unless a row says otherwise; cell (c) rows use the verified 3-turn 42/52/62 chain
(`scripts/cellc.sh`; 160-token budget, 192 on thinking models, 384 on reasoning-heavy
Laguna — see its section); divergence rows (c4/c5)
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

**Summary.** Eight models measured end to end across five cells each: three 35B-class MoE
hybrids ~2× host RAM (Qwen3.5-family: Ornith 1.5, Cyber-Tiel-Coder, Qwen3.6), one 8B
hybrid that fits RAM comfortably (LFM2.5), and four plain-transformer MoEs (Laguna-XS,
18.9 GB; Qwen3-30B-A3B, 18.6 GB — both ~2× RAM; Ling-mini-2.0, 9.9 GB barely fitting;
OLMoE-1B-7B, 4.0 GB comfortably).
Where the model is past RAM, streaming (a → b) roughly doubles decode (1.39 → 2.06,
1.29 → 2.19, 1.37 → 1.95, 2.44 → 3.59, 1.43 → 2.05 tok/s) while major faults collapse
(470 → 129, 616 → 72, 642 → 65, 340 → 1.9, 533 → 3.7 per token): the mmap baseline
thrashes, the streamed run reads — and on Laguna, Qwen3-30B and Qwen3.6 prefill speeds
up too (47.5 → 34.1, 44.5 → 22.1, 31.9 → 19.0 s), since a ~2×-RAM baseline thrashes its
prefill as well. Where the model fits, streaming
loses: −11% decode on LFM2.5 (0 faults both ways), −6% on OLMoE (its 3.7 GiB cache
coexists with the 4 GB model), and −31% on Ling-mini, where the streaming stack's own
cache (7.5 GiB) pushed a 9.9 GB model into swap (0.86 → 19.3 faults/token) — the cache
became the memory pressure. The fits-RAM verdict is set by the model-plus-cache sum,
not the model alone. The (b) stack is a tool for memory
pressure, not a default. Session residency (c rows) removes most of the per-turn prefill:
append reuse with `--auto-echo` reuses 22–310 tokens per follow-up turn (prefill to
~1 s), and snapshot rollback (`--rs-seq 64`) turns a hybrid's worst case — a divergence
the client causes by not echoing reasoning back — from a full clear into a bounded rewind
on every hybrid whose rewind depth fit the budget: 33/23 (Ornith), 33/203 (Cyber-Tiel),
26/22 (LFM2.5), answers verified, no degeneration, with divergence-turn prefill halving
on Cyber-Tiel (42.6 → 19.3 s) and dropping 2.3× on LFM2.5 (2.82 → 1.25 s). On Qwen3.6
the divergence needed depth 135 — past a heavy thinker's whole turn — and the 64-plane
budget refused honestly to a full clear (the documented worst case, observed live). On the plain transformers, the same
turns are free partial chops without any of it (c4 reuses 55, 34 and 22 with rs-seq
off), so the divergence tax `--rs-seq` removes is a hybrid phenomenon.

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
- Rewind depth is per-turn, and a heavy thinker's turn can outrun the budget. The
  divergence rewind must reach past the whole previous turn: Qwen3.6's (a ~105-token
  think span plus the reply) needed depth 135 — the 64-plane pool refused honestly and
  the turn full-cleared (the documented worst case, first observed live; the refusal
  costs a ~6 s restore attempt). Snapshot budgets bound turn depth, not context;
  `--rs-seq` still buys the warmup composition (Qwen3.6 T1: 2/22 at 2.6 s).
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
  follow-up turn;the qwen35 family pays one reconcile turn first (T2 stays a full
clear, T3+ rides the cache), and echo-style reuse never engages on it without the
bridge — only the snapshot rewind does. On Qwen3.6 the bridge engaged but the rewrite
never reconciled at all (payloads ballooned 52 → 350, reuse 0 everywhere) — same arch
family as Cyber-Tiel, different chat template: the reconcile is template-sensitive,
and a new model's first follow-up `n_reused` is the thing to check.The laguna and qwen3moe templates render history reasoning natively,
which makes `--auto-echo` redundant there (Qwen3-30B is also the matrix's heaviest
thinker — its c-suite needed MAXTOK=768; 192/384 truncated its think spans): plain
clients already reuse, and the rewrite itself costs a reconcile turn. A non-thinking model (Ling-mini) has nothing
  to echo: reuse is native and `--auto-echo` is a verified no-op.
- The divergence tax is a hybrid phenomenon, not a universal one. The plain-transformer
  MoEs reuse 55 (Laguna), 34 (Ling-mini), 22 (Qwen3-30B) and 34 (OLMoE) tokens on the
divergence turn with rs-seq OFF — the generic diff path chops freely — while every hybrid
  full-clears the same shape. `--rs-seq` matters exactly on the archs that cannot rewind without snapshots.
- Every reuse zero in the matrix is either a designed full clear or a structural
  template/echo mismatch — never a wrong answer. All cell answers are verified (42/52/62;
  a fast-but-wrong run reports FAIL).

**Recommendations for use.**

- Above-RAM models, raw generation: the (b) stack — `--moe-stream --cache-mb auto
  --io-threads 4 --overlap --dense-weights anon`. Lossless, measured here at +42–70%
  decode over mmap with far fewer major faults (1.9–129 vs 65–642 per token).
- Models that fit RAM: plain mmap, no streaming flags. Measured at +12% decode on
  LFM2.5 (9.69 vs 8.64 tok/s, ~5× faster prefill) and +46% on Ling-mini (12.59 vs
  8.63) — where the streaming stack's expert cache itself pushed a 9.9 GB model into
  swap (0.86 → 19.3 faults/token). Streaming for a resident model is pure overhead.
- Agent / OpenAI-client serving: run the bridge with `--auto-echo` and default warmup on.
  Expect follow-up-turn prefill to collapse to tens of tokens (from the first follow-up
  on LFM2.5-class templates; after one reconcile turn on the qwen35 family; on
  laguna/qwen3moe-class templates (native history-reasoning rendering) leave
  `--auto-echo` off — plain history already reuses and the echo rewrite costs a turn;
  on non-thinking models (Ling-mini, OLMoE) it is a no-op; on Qwen3.6 the rewrite engaged but never
  reconciled — verify the first follow-up's `n_reused` before trusting echo on a new
  template). Divergence-shaped
  turns (aider edit turns, clients that drop reasoning) still full-clear on hybrids
  unless `--rs-seq` is on; on plain-transformer MoEs they reuse without it.
- Thinking hybrids with clients that do not echo reasoning: enable `--rs-seq 64`
  (experimental, off by default). Budget by rewind depth, not context length: 64 planes
  ≈ 3.9 GiB on a 35B — larger budgets OOM a small host without buying anything, since a
  depth beyond budget refuses honestly and full-clears anyway. Depth is set by the
  longest turn you must rewind past: Qwen3.6's single turn needed 135 planes, so 64
  bought only the warmup composition there — check a refused rollback's `depth=` line
  before assuming the budget covers a heavy thinker.
- Prefill rarely speeds up under streaming: prefill routes nearly all experts, so there
  is nothing to skip (cell b prefill 68.3 s vs a's 61.8 s on the coder). The exception
  is the ~2×-RAM regime, where the mmap baseline thrashes its prefill too and
  streaming's bounded reads win (Laguna 47.5 → 34.1 s, Qwen3-30B 44.5 → 22.1, Qwen3.6
  31.9 → 19.0). The streaming win is decode throughput and stability.

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

## Laguna-XS-2.1 (arch `laguna`, MoE, 18.9 GB, ~2× host RAM — the matrix's first NON-hybrid)

Upstream classifies `laguna` as a plain-transformer MoE — it is in neither the hybrid
nor the snapshot-rollback arch lists (verified in `llama-arch.cpp`), and the gguf has no
recurrent/conv state keys or tensors, only standard attention with a sliding window. Every
difference below follows from that classification.

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `8958e88` | 41.9 | 47.5 | **1.37** | — | — | **642** |
| b) bmoe streaming | arc `8958e88` | 113.0 | **34.1** | **1.95** | 76.0 MiB | 82.1% | 65 |
| c) warmup off | `bench/host-rs@2a8d47ac9` | — | 30.4 (T1) | 1.9–2.0 | — | — | — |
| c) warmup on | `bench/host-rs@2a8d47ac9` | — | **1.28 (T1, ~24×)** | 2.0–2.9 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | 44.0 (T2) / **13.7 (T3)** | 2.7 (T3) | **T2: 252 prompt / 55 reused; T3: 25 prompt / 310 reused** | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 28.0 / 10.9 (T2) | 2.0 / 2.0 | **T2: 28 prompt / 55 reused** | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 1.45 / 11.0 (T2) | 2.0 / 2.2 | **T1: 1 prompt / 54 reused; T2: 28 prompt / 55 reused** | — | — |

Reading the cells:

- **(a) → (b)**: +42% decode with faults collapsing 642 → 65 — and for the first time
  in this matrix **prefill speeds up too** (47.5 → 34.1 s): on a ~2×-RAM model the
  baseline's prefill thrashes just like its decode, and streaming reads instead.
- **(c4) divergence without rs-seq reuses 55 tokens** — a hybrid full-clears the same
  turn. On a plain transformer the generic diff path is a free partial chop, so the
  "divergence tax" that `--rs-seq` removes on hybrids does not exist here. (c5) with
  `--rs-seq 64` is behaviourally identical (28/55): the snapshot pool is simply unused,
  which is the clean confirmation of the classification above.
- **(c) warmup**: T1 prefill 30.4 → 1.28 s (~24×) — the replay composes with the first
  turn via the same free suffix-chop (1 fresh / 54 reused). On hybrids this composition
  needs `--rs-seq`; on a transformer it is automatic.
- **(c) auto-echo is redundant on this template**: Laguna renders history reasoning
  natively, so plain clients (c1) already reuse across turns without echoing anything
  back. Rewriting the history to echoed form buys one reconcile turn (T2: 252 fresh
  prompt tokens, 44 s — the one expensive row in the c-suite) and its visible win is
  decode-side (T3 2.75 vs 1.87 tok/s). Recommendation: leave `--auto-echo` off on
  laguna-class templates; it exists for templates that strip or alter reasoning.
- **Reasoning-heavy budget note**: 192- and 256-token budgets truncated Laguna's think
  spans mid-reasoning (cellc verdict FAIL with empty content — measured, then re-run at
  384, which all cells pass). bmoe-cli has no reasoning-budget wiring yet (upstream's
  `common/reasoning-budget.h` is unwired in our CLI), so the honest lever today is the
  request's `max_tokens`.

## Ling-mini-2.0 (arch `bailingmoe2`, MoE, 9.9 GB — barely fits host RAM; the edge case)

Second plain-transformer MoE in the matrix (`bailingmoe2` is in neither the hybrid nor
the snapshot-rollback arch lists; the rollback list's BAILINGMOE3 is a different arch).
Non-thinking model — its replies carry no reasoning span, which shapes the c-suite below.

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `b7f3cd0` | 21.3 | 2.26 | **12.59** | — | — | 0.86 |
| b) bmoe streaming | arc `b7f3cd0` | 5.0 | 12.75 | 8.63 | 11.0 MiB | 91.6% | **19.27** |
| c) warmup off | `bench/host-rs@2a8d47ac9` | — | 7.9 (T1) | 3.0–10.2 | — | — | — |
| c) warmup on | `bench/host-rs@2a8d47ac9` | — | **0.80 (T1, ~10×)** | 4.0–11.0 | — | — | — |
| c) warmup + auto-echo | `bench/host-rs@2a8d47ac9` | — | **1.07 (T3)** | 12.0–12.7 (T2/T3) | **T2: 27 prompt / 34 reused; T3: 27 prompt / 63 reused** | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 34.4 / 4.94 (T2) | 1.0 / 5.3 | **T2: 27 prompt / 34 reused** | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 0.13 / **1.64 (T2)** | 9.4 / 7.5 | **T1: 1 prompt / 31 reused; T2: 27 prompt / 34 reused** | — | — |

Reading the cells:

- **(a) → (b)**: the sharpest fits-RAM result in the matrix — streaming LOSES 31%
  decode (12.59 → 8.63 tok/s) and prefill runs 5.6× slower, and for a clean reason:
  at 9.9 GB the mmap baseline stays essentially resident (0.86 faults/token), while the
  streaming stack's expert cache (7.5 GiB) plus its anon dense copy tip the host into
  swap (19.27 faults/token). Faults went UP, not down — the cache itself became the
  memory pressure. Confirms the recommendation: streaming is for at-or-past-RAM models.
- **(c) suite, second plain transformer, now a non-thinking one**: reuse engages from
  the first follow-up with no echo and no reconcile turn (c1 T2 already 27 prompt / 34
  reused), `--auto-echo` is a verified no-op here (c3 ≈ c2, plus decode gains from the
  warm cache), divergence turns are free partial chops with rs-seq OFF (c4 27/34), c5 ≡
  c4 (snapshot pool unused — same clean classification confirmation as Laguna), and
  warmup composes freely (T1 1 fresh / 31 reused, 0.08–0.8 s across cells).
- **(c) note**: c4's T1 prefill (34.4 s at 1.0 tok/s) is the one outlier row — the
  cold-load cell racing the earlier cells' page cache; its T2 figure (4.94 s, 27/34) is
  the comparable one and matches the suite.

## Qwen3-30B-A3B (arch `qwen3moe`, MoE, 18.6 GB, ~2× host RAM — the reference thinker)

Third plain-transformer MoE in the matrix (`qwen3moe` is on neither the hybrid nor the
snapshot-rollback arch lists) and its heaviest reasoner: r1's think span ran 1313
characters before terminating — the MAXTOK ladder needed two rungs (192 and 384 both
truncated r1: empty content, FAIL by design; 768 passes). Its template renders history
reasoning natively.

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `9624b13` | 117.7 | 44.5 | 2.44 | — | — | **340.4** |
| b) bmoe streaming | arc `9624b13` | 95.1 | 22.1 | **3.59** | 40.3 MiB | 93.4% | 1.91 |
| c) warmup off (c1) | `bench/host-rs@2a8d47ac9` | — | 24.3 (T1) | 2.2–2.4 | — | — | — |
| c) warmup on (c2) | `bench/host-rs@2a8d47ac9` | — | **1.23 (T1, ~20×)** | 2.1–2.4 | — | — | — |
| c) warmup + auto-echo (c3) | `bench/host-rs@2a8d47ac9` | — | 1.04 (T1) | 2.0–2.4 | T2: 28 prompt / 22 reused; T3: 28 prompt / 50 reused | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 1.16 / 10.2 (T1/T2) | 2.2 / 2.3 | T1: 1 prompt / 21 reused; T2: 28 prompt / 22 reused | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 0.75 / **9.5 (T2)** | 2.4 / 2.4 | T1: 1 prompt / 21 reused; T2: 28 prompt / 22 reused | — | — |

Reading the cells:

- **(a) → (b)**: the at-RAM pairing again — the baseline thrashes (340.4 faults/token),
  streaming holds the model (1.91) with a 93.4% hit: decode +47% (2.44 → 3.59 tok/s) and
  prefill **halved** (44.5 → 22.1 s) — the second arch after Laguna where streaming
  speeds up prefill too, for the same reason: a ~2×-RAM baseline thrashes its prefill as
  well, while the streamed run's 40.3 MiB/token expert reads stay ahead of compute.
- **(c) suite, third plain transformer, now a heavy thinker**: every classification
  prediction reproduces. Native reuse from the first follow-up (c1 T2 already 28 prompt /
  22 reused — no echo, no reconcile turn; unlike the qwen35-family hybrids),
  `--auto-echo` a verified no-op (c3 ≈ c2), divergence turns free partial chops with
  rs-seq OFF (c4 T2 28/22), c5 ≡ c4 (snapshot pool unused — third clean classification
  confirmation), and warmup composing freely (T1 1 fresh / 21 reused at 0.7–1.2 s vs
  c1's 24.3 s, ~20×).
- **(c) protocol note**: c1 is the only warmup-off cell in the suite — the warmup replay
  runs in every later cell (c2–c5), so their T1 rows (1 prompt / 21 reused) are
  warm-start by construction and are the composition rows, not warmup-off baselines.

## Qwen3.6-35B-A3B (arch `qwen35moe`, MoE, 22.1 GB, ~2× host RAM — the matrix's deepest thrash and its first honest rewind refusal)

Third `qwen35moe` hybrid in the matrix (after Ornith and Cyber-Tiel) and a heavier
thinker than both: the c-suite ran at MAXTOK=192 (r1's think span terminated at 419
chars), but its T1 turn is long enough that the divergence rewind — which must reach
past the whole previous turn — exceeded the 64-plane snapshot budget.

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `dd43788` | 93.2 | 31.9 | 1.43 | — | — | **533.0** |
| b) bmoe streaming | arc `dd43788` | 65.1 | 19.0 | **2.05** | 53.9 MiB | 87.4% | 3.68 |
| c) warmup off (c1) | `bench/host-rs@2a8d47ac9` | — | 17.9 (T1) | 1.4–1.6 | — | — | — |
| c) warmup on (c2) | `bench/host-rs@2a8d47ac9` | — | 13.2 (T1) | 1.5 | — | — | — |
| c) warmup + auto-echo (c3) | `bench/host-rs@2a8d47ac9` | — | 35.7 (T2) | 1.3–1.6 | **T2/T3: echo ENGAGED, reuse 0** | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 15.5 (T2) | 1.5 | **T2: 52 prompt / 0 reused** | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 21.9 (T2) | 0.84 | **T1: 2 prompt / 22 reused; T2: rewind REFUSED (depth 135 > 64), 52 / 0** | — | — |

Reading the cells:

- **(a) → (b)**: the deepest thrash in the matrix (533 faults/token — 22.1 GB is the
  largest model measured) and the biggest relative fault collapse (533 → 3.7, ~145×):
  decode +43% (1.43 → 2.05 tok/s) and prefill 1.7× faster (31.9 → 19.0 s) — the third
  arch where streaming speeds up prefill too, for the same reason: a ~2×-RAM baseline
  thrashes its prefill as well.
- **(c) suite, a qwen35moe hybrid with two new wrinkles**:
  - Without echo every turn full-clears (c1: 24/0 → 52/0 → 80/0) — the qwen35-family
    shape. Warmup buys cold-start only (T1 prefill 17.9 → 13.2 s, reuse still 0).
  - **`--auto-echo` engaged but never reconciled**: the rewrite is visibly in the
    payloads (T2/T3 prompts balloon 52 → 185 → 350) yet `n_reused` stays 0 at every
    turn — Qwen3.6's template renders the echoed reasoning differently from what the
    rewrite produces. Same arch family as Cyber-Tiel (echo reuse engaging from T3),
    different chat template: the reconcile is template-sensitive, not arch-sensitive.
    First matrix model where echo costs prefill and buys nothing.
  - **c5 is the matrix's first live rewind refusal**: T1 still composes warmup +
    snapshots (2 prompt / 22 reused, 2.6 s vs c2's 13.2 s full-clear T1), but T2's
    divergence needs rewind depth 135 — past Qwen3.6's whole T1 turn (a ~105-token
    think span plus the reply) — and the 64-plane budget cannot reach:
    `seq_rm: rollback refused: seq=0 depth=135 pending=0 want_idx=2 …`. The engine
    refuses honestly and full-clears (52/0 ≡ c4; the refused path pays ~6 s of restore
    attempt before clearing: T2 prefill 21.9 vs 15.5 s). Correctness is never at
    risk — this is the documented worst case, now observed live: a heavy thinker's
    single turn can exceed any affordable snapshot budget.
- Bridge-mode decode is slower than the CLI cells (0.84–1.6 tok/s): short answers,
  reasoning headroom and per-request overhead are included there (same caveat as
  Ornith).

## OLMoE-1B-7B (arch `olmoe`, MoE, 4.0 GB, fits host RAM comfortably — completes the fits-RAM trio)

Fourth plain-transformer MoE in the matrix and its last queued model: `olmoe` is on
neither the hybrid nor the snapshot-rollback arch lists (verified in llama-arch.cpp),
and the behavior below matches. Non-thinking model — r1 is a plain sentence with no
reasoning span.

| Cell | Engine commit | load s | prefill s | tok/s | flash/token | cache hit | majflt/tok |
|---|---|---:|---:|---:|---:|---:|---:|
| a) mmap baseline | arc `dd43788` | 8.2 | 1.67 | **10.75** | — | — | 0.00 |
| b) bmoe streaming | arc `dd43788` | 1.8 | 7.81 | 10.13 | 2.3 MiB | 97.0% | 0.00 |
| c) warmup off (c1) | `bench/host-rs@2a8d47ac9` | — | 6.6 (T1) | 5.4–11.9 | T2: 26 prompt / 34 reused; T3: 26 prompt / 69 reused | — | — |
| c) warmup on (c2) | `bench/host-rs@2a8d47ac9` | — | **0.12 (T1, ~56×)** | 8.1–9.8 | T2: 26 prompt / 36 reused; T3: 26 prompt / 74 reused | — | — |
| c) warmup + auto-echo (c3) | `bench/host-rs@2a8d47ac9` | — | 0.10 (T1) | 11.2–11.9 | T2: 26 / 36; T3: 26 / 74 — ≡ c2 (no-op) | — | — |
| c4) divergence, rs-seq off | `bench/host-rs@2a8d47ac9` | — | 6.9 / 0.99 (T1/T2) | 6.3 / 11.5 | T2: 26 prompt / 34 reused | — | — |
| c5) divergence, rs-seq 64 | `bench/host-rs@2a8d47ac9` | — | 0.07 / 0.94 (T1/T2) | 10.1 / 12.6 | T1: 1 prompt / 24 reused; T2: 26 / 34 — ≡ c4 (pool unused) | — | — |

Reading the cells:

- **(a) → (b)**: the mildest fits-RAM penalty in the matrix (−6% decode, 10.75 →
  10.13 tok/s) with **zero faults in both cells**: the 3.7 GiB expert cache coexists
  with the 4 GB model without pushing anything into swap. Compare Ling-mini, where the
  same cache pushed a 9.9 GB model into swap (0.86 → 19.3 faults/token): the fits-RAM
  verdict is set by the **model-plus-cache sum**, not the model alone.
- **(c) suite, fourth plain transformer, non-thinking**: native reuse from the first
  follow-up (T2 26/34, T3 26/69 at ~1 s prefill), `--auto-echo` a verified no-op
  (c3 ≡ c2 — nothing to echo), divergence a free partial chop (c4 26/34 with rs-seq
  OFF), c5 ≡ c4 (snapshot pool unused), and warmup composing freely at **~56×** — the
  largest warmup ratio in the matrix (6.6 s → 0.12 s; the whole conversation reuses
  natively, so the replayed prefix is almost entirely recoverable).
- Answers verified 42/52/62 in every cell.

Queued next: none — the eight-model host queue is complete; see the Summary for the
cross-model picture.
