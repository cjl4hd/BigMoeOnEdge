# ADR-004: Hybrid residency blockers — upstream PR, not a fork branch (for now)

**Status:** Accepted (2026-09-18)

## Context

Aider's confirmed telemetry shows the residency ladder working on a thinking hybrid:
non-edit turns prefill 16–18 tokens with `n_reused` growing 587 → 1243, while edit turns
full-clear (~815 prefilled, ~36 s). The edit-turn rewind-to-common-prefix would roughly
halve that (~390 reused, ~425 re-prefilled, ~18 s) — and it needs recurrent-state
snapshot rollback (`--rs-seq`), which is blocked by two upstream defects:

1. **lfm2moe crashes at graph reserve** with snapshots enabled: a fixed ~368-byte
   node-pool shortfall (`needed 836640, available 836272`), invariant to context,
   ubatch and budget. Upstream's reserve estimate under-counts the snapshot ops the
   LFM2 graph emits — a small, well-understood bug in an arch that is already on
   upstream's rollback allowlist.
2. **Snapshot restore is not bit-exact**: proven on Qwen3.5-9B under greedy decoding
   (identical prompt: `40` fresh vs `420` restored). The gated delta net accumulates
   state in chunks; floating-point recurrence is order-sensitive, so a mid-chunk
   snapshot restores state a fresh prefill would never produce. Fixing it means
   redesigning snapshot semantics (chunk-boundary snapshots + partial-chunk replay) —
   research-grade work in novel attention code, which is why upstream marks the
   mechanism `[EXPERIMENTAL]`.

Both fixes live in llama.cpp, which this project deliberately does not fork (the
submodule points at `Helldez/llama.cpp`, a mirror carrying exactly one sanctioned
1-commit expert hook). The payoff is also bounded: the file content an edit changes can
never be reused, so the rewind halves edit turns — while non-edit turns already cost
~0.8 s thanks to append reuse.

## Decision

1. **The reserve-crash fix is pursued as an upstream PR against
   `ggml-org/llama.cpp`** (mainline) — not against `Helldez/llama.cpp`, which is only
   the submodule's fork remote. Upstreaming removes the blocker for everyone and costs
   no fork maintenance; the arch is on their allowlist, so the bug blocks their own
   feature.
2. **Bit-exact restore is not attempted on a fork now.** If a fork experiment is ever
   warranted, it goes as a separate 1-commit branch on `Helldez/llama.cpp` (the
   submodule remote) with maintainer agreement per AGENTS.md rule 1 — never a new
   personal fork and never an in-tree diff. The trigger to revisit: upstream landing
   chunk-boundary snapshots or an equivalent exactness fix; then `--rs-seq` flips on
   with no residency-code changes (ADR-001 §2).
3. Until both land, hybrid edit turns full-clear — accepted, because the alternative
   (non-bit-exact restore) silently corrupts outputs.

## Consequences

- Easier: no fork maintenance, no bump cost; the reserve PR is cheap and unblocks
  experimentation on lfm2moe even before exactness is solved.
- Harder: hybrid edit turns stay at ~36 s on this host until upstream fixes land —
  timeline outside our control.
- Accepted as-is: pure-transformer models (Ling-mini) already get the edit-turn rewind
  for free via the generic diff path; the `40` vs `420` measurement stands as the
  standing proof of why default-off is correct.
