# SESSION_SUMMARY — entry point for every session

*Last rewritten: 2026-09-18. Phase: hybrid session residency (feature branch, pre-PR).*
*One-line status: all residency work committed and pushed to the fork; gates green; bridge serving Ling-mini on :8017; stacked PR not yet opened.*

## State delta (what this arc proved — details in `docs/adr/` and RE_PROGRESS.md)

- **Hybrid reuse policy shipped** (ADR-001): append-only reuse for hybrids is safe and
  merged-quality; snapshot rollback (`--rs-seq`, default **0**) wired but off — restore is
  provably not bit-exact (greedy decode gave `40` fresh vs `420` restored on Qwen3.5-9B).
- **Reasoning echo shipped** (ADR-002): `preserve_reasoning` request flag keeps reasoning
  resident on thinking hybrids. Measured on LFM2.5: T2/T3 prefill only ~17 tokens while
  reuse grows 101→235; answers correct; decode ~9.2 tok/s.
- **Three upstream gates documented** in `docs/serve.md`: llama.cpp #25913 open (hybrid
  state restore worth zero), snapshot restore not bit-exact, LFM2 graph-reserve crash
  with snapshots (fixed ~368-byte node-pool shortfall, invariant to every knob).
- **Template discoveries**: LFM2.5 natively supports `preserve_thinking` template kwargs;
  Qwen3.5 bakes an empty `<think>` span into the generation prompt that re-renders omit,
  so its history can never append-extend the mirror.

## Artifacts touched (this arc)

| File | What |
|---|---|
| `core/src/engine/session.cpp` | append-reuse path + mirror poisoning on every fail/cancel path; `preserve_reasoning` → template kwargs |
| `core/include/bmoe/{config.h,session.h}` | `n_rs_seq`, `preserve_reasoning` fields (RunConfig + SessionConfig, mapped in `runtime.cpp`) |
| `cli/main.cpp` | `--rs-seq N` flag + protocol field plumbing |
| `scripts/bmoe-serve.py` | per-request `think` + `preserve_reasoning` on both HTTP handlers |
| `docs/serve.md` | three upstream gates + template-divergence findings |
| `docs/adr/001…002`, `docs/README.md`, `CHANGELOG.md`, this file, `RE_PROGRESS.md` | decisions + lifecycle docs |

Commits (branch `feat/session-residency`, pushed to `fork`): `e930b4d` append-reuse ·
`63768e7` --rs-seq · `5e664bf` LFM2 reserve-crash doc · `d83d153` preserve_reasoning.
Tags: `progress/2026-09-17-residency-warmup`→`ff760fd`, `progress/2026-09-17-snapshot-rollback`→`63768e7`.

## Environment state

- **Server running**: `scripts/bmoe-serve.py` on Ling-mini-2.0 (`bailingmoe2`), port 8017,
  8k ctx, `--chatml`. Check: `curl -s http://127.0.0.1:8017/v1/models`. Model id `bmoe-local`.
- **Models on disk** (`~/llm/models/`): Ling-mini-2.0 (daily driver), LFM2.5-8B (hybrid,
  the echo-verification subject), Qwen3.5-9B (dense hybrid, 5.7 GB), olmoe-1b-7b,
  Laguna-XS-2.1, Ornith-1.5, Qwen3-30B, Qwen3.6-35B, Cyber-Tiel-35B. No downloads pending.
- **Remotes**: `origin` = Helldez/BigMoeOnEdge (upstream, PR #197 open from fork's
  `feat/serve-bridge-arm64`), `fork` = cjl4hd/BigMoeOnEdge (push target).
  Untracked and NOT ours: `.opencode/`, `bmoe-arm64*`, `opencode.json`, `.aider*`, logs.
- Ephemeral: `/tmp/bmoe-serve.log` (server log); regenerate by relaunching the server (below).

## Open questions / blocked items

1. **Stacked PR not opened** — base branch `feat/serve-bridge-arm64` exists only on the
   fork, so the PR must be fork-internal (`cjl4hd:feat/serve-bridge-arm64` →
   `cjl4hd:feat/session-residency`), retargeted to upstream `main` after #197 merges.
   `gh` is installed and authenticated as `cjl4hd`.
2. When will upstream make snapshot restore bit-exact? (Watch #25913 and the
   `[EXPERIMENTAL]` markers; then flip `--rs-seq` default — ADR-001 §2.)
3. Bridge-side auto-echo of reasoning would make `preserve_reasoning` work with
   unmodified OpenAI clients — designed, not implemented (ADR-002 consequences).

## Next actions (ordered)

1. **Verify the append payoff case, then open the stacked PR.** `gh pr create --repo
   cjl4hd --base feat/serve-bridge-arm64 --head feat/session-residency`. If #197 merges
   first, retarget to `upstream main` instead.
2. **Bridge auto-echo prototype**: inject echoed reasoning into assistant history in the
   bridge so any client gets append reuse (ADR-002).
3. **Re-run the aider two-turn benchmark on LFM2.5** to quantify the end-to-end win in a
   real agentic client.
4. **Daily driver**: keep Ling-mini on :8017. Relaunch command:
   `setsid nohup python3 -u scripts/bmoe-serve.py -m ~/llm/models/Ling-mini-2.0-Q4_K_M.gguf --engine-args "--ctx-size 8192 --chatml" --port 8017 > /tmp/bmoe-serve.log 2>&1 &`

## Resume gates (run all; all must assert positives)

1. `cmake --build build -j4 2>&1 | grep -E 'error|warning'` → empty output (clean build).
2. `cd build && ctest --output-on-failure` → **13/13 passed** (byte-identity).
3. `git status -sb` → `## feat/session-residency...fork/feat/session-residency` (in sync,
   no unstaged tracked changes) and `git log --oneline -1` → `d83d153`.
4. `curl -fsS -m 3 http://127.0.0.1:8017/v1/models` → returns the `bmoe-local` JSON.

If a gate fails: re-derive from artifacts (git log, docs/adr, RE_PROGRESS.md) before
continuing. Never weaken a gate to make it pass.
