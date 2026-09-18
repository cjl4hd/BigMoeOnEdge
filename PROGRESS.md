# PROGRESS — entry point for every session

This file holds the **resume section** (below, rewritten every session — never append).
The append-only history log lives in [PROGRESS.history.md](PROGRESS.history.md);
the trust hierarchy is: resume section > history log > older sections of either.

*Last rewritten: 2026-09-18. Phase: hybrid session residency — features shipped and
proven; **stacked PR held until serve-bridge PR #197 merges** (user decision).*
*One-line status: `scripts/bench-features.sh` validates all three residency features
end-to-end; lifecycle docs migrated to the single-`PROGRESS.md` convention; server on
Ling-mini :8017.*

## State delta (this session)

- **Feature-flag audit answered:** the residency features are deliberately NOT all CLI
  flags — warmup is a bridge-level default (`--no-warmup` to disable), reasoning echo is a
  per-request field (`preserve_reasoning`), `think` is per-request; **`--rs-seq` is the only
  new CLI flag** (default 0 = off). Residency itself is always-on engine behavior.
- **`scripts/bench-features.sh` added and validated end-to-end** (Ling-mini): warmup cuts
  T1 prefill 2.19 s → 0.06 s (~35×), tok/s 9.4 → 16.1; echo scenario skipped *behaviorally*
  (no `reasoning_content`); `--rs-seq` verified a no-op on pure transformers with correct
  answers. Measured table lives in `docs/serve.md`.
- **Lifecycle convention migrated** (user choice): one `PROGRESS.md` (resume section on
  top, history below) replaces the two-file scheme; `SESSION_SUMMARY.md` → `PROGRESS.md`,
  `RE_PROGRESS.md` → `PROGRESS.history.md`. Both home skill copies
  (`~/skills/skills/`, `~/.agents/skills/`) updated and synced — the `~/.agents/` one was
  stale (missing the SOFTWARE_REQUIREMENTS module).
- **Stacked-PR plan recorded and held** until PR #197 merges — see Open questions.

## Artifacts touched (this session)

| File | What |
|---|---|
| `scripts/bench-features.sh` (new, executable) | before/after benchmark: warmup / echo / `--rs-seq`, isolated port 8019, verified answers (42/52/62), CSV under `BENCH_OUT` (default `.bench-features/`, regenerable — never commit it) |
| `PROGRESS.md` / `PROGRESS.history.md` | renamed from `SESSION_SUMMARY.md` / `RE_PROGRESS.md`, restructured per the updated skill |
| `docs/serve.md` | `bench-features.sh` section + the measured Ling-mini table |
| `CHANGELOG.md` | bench-features entry |
| `~/skills/skills/project-lifecycle/SKILL.md` + `~/.agents/` copy | skill now prescribes the single-`PROGRESS.md` convention (outside the repo) |

Branch `feat/session-residency`, pushed to `fork`. Earlier arc commits: `e930b4d`
append-reuse · `63768e7` --rs-seq · `5e664bf` LFM2 reserve-crash doc · `d83d153`
preserve_reasoning. Tags: `progress/2026-09-17-residency-warmup` (`ff760fd`),
`progress/2026-09-17-snapshot-rollback` (`63768e7`), `progress/2026-09-18-reasoning-echo`
(`d83d153`).

## Environment state

- **Server running**: `scripts/bmoe-serve.py` on Ling-mini-2.0 (`bailingmoe2`), port 8017,
  8k ctx, `--chatml`. Check: `curl -s http://127.0.0.1:8017/v1/models` → model id `bmoe-local`.
  Relaunch: `setsid nohup python3 -u scripts/bmoe-serve.py -m ~/llm/models/Ling-mini-2.0-Q4_K_M.gguf --engine-args "--ctx-size 8192 --chatml" --port 8017 > /tmp/bmoe-serve.log 2>&1 &`
- **Models on disk** (`~/llm/models/`): Ling-mini-2.0 (daily driver), LFM2.5-8B, Qwen3.5-9B,
  olmoe-1b-7b, Laguna-XS-2.1, Ornith-1.5, Qwen3-30B, Qwen3.6-35B, Cyber-Tiel-35B.
- **Global warmup cache** `~/.cache/bmoe-serve/warmup.json`: holds the user's LFM2.5 math
  chain; `bench-features.sh` backs it up and restores it automatically (incl. Ctrl-C).
- **Remotes**: `origin` = Helldez/BigMoeOnEdge (upstream; PR #197 open from fork's
  `feat/serve-bridge-arm64`), `fork` = cjl4hd/BigMoeOnEdge (push target). `gh` authed as `cjl4hd`.
- Untracked and NOT ours: `.opencode/`, `bmoe-arm64*`, `opencode.json`, `.aider*`, logs.
- Ephemeral: `/tmp/bmoe-serve.log`, `/tmp/bf-test/` (bench dry-run output), `.bench-features/`.

## Open questions / blocked items

1. **Stacked PR is HELD until #197 merges** (user decision, 2026-09-18). The plan, ready
   to execute: (a) verify `feat/session-residency` still contains `feat/serve-bridge-arm64`
   (`git merge-base --is-ancestor`); (b) when #197 merges to upstream `main`, sync fork
   main, rebase `feat/session-residency` onto it — the serve-bridge commits collapse into
   main so the PR becomes a clean diff; (c) `gh pr create --repo Helldez/BigMoeOnEdge
   --base main --head cjl4hd:feat/session-residency`. Fallback if #197 stalls long: open
   the fork-internal stack now (`--repo cjl4hd --base feat/serve-bridge-arm64`) and
   retarget later.
2. When will upstream make snapshot restore bit-exact? (#25913 + `[EXPERIMENTAL]`
   markers; then flip `--rs-seq` default — ADR-001 §2.)
3. Bridge auto-echo of reasoning would make `preserve_reasoning` work with unmodified
   clients — designed, not implemented (ADR-002).

## Next actions (ordered)

1. **Watch PR #197**; when merged, execute the stacked-PR plan in Open questions 1.
2. **Run `scripts/bench-features.sh` on LFM2.5** to capture the echo off/on rows in the
   doc table (the one measurement the script adds beyond the manual 3-turn run):
   `BENCH_OUT=/tmp/bf-lfm scripts/bench-features.sh ~/llm/models/LFM2.5-8B-A1B-UD-Q4_K_M.gguf`
3. **Bridge auto-echo prototype** (ADR-002): inject echoed reasoning into assistant
   history in the bridge.
4. **Aider two-turn benchmark on LFM2.5** to quantify the end-to-end agent win.
5. **Daily driver**: keep Ling-mini on :8017 (relaunch command above).

## Resume gates (run all; all must assert positives)

1. `cmake --build build -j4 2>&1 | grep -E 'error|warning'` → empty output (clean build).
2. `cd build && ctest --output-on-failure` → **13/13 passed** (byte-identity).
3. `git status -sb` → `## feat/session-residency...fork/feat/session-residency` with a
   clean tree (no unstaged tracked changes), and `git log --oneline -1` is the newest
   commit of the residency arc (`git log --format=%s -1` mentions bench-features).
4. `curl -fsS -m 3 http://127.0.0.1:8017/v1/models` → returns the `bmoe-local` JSON.
5. `bash -n scripts/bench-features.sh` → silent (script parses).

If a gate fails: re-derive from artifacts (git log, docs/adr, PROGRESS.history.md) before
continuing. Never weaken a gate to make it pass.
