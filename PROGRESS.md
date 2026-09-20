# PROGRESS — session entry point + evidence log

**Structure (per the project-lifecycle skill):** the resume section below is rewritten
every session — never append to it. Under the divider, the append-only history log holds
the long-form evidence narrative; entries are never rewritten, only falsified explicitly
by newer entries. Trust hierarchy: resume section > history > older sections of either.
Log opened 2026-09-18; earlier project history lives in `CHANGELOG.md` and `git log`.

*Resume last rewritten: 2026-09-19 (session 9 wrap-up). Phase: host bench campaign —
five models published (Ornith, Cyber-Tiel, LFM2.5, Laguna, Ling-mini); queue head is
Qwen3-30B.*
*One-line status: arc pushed (`8958e88..b7f3cd0`); Ling-mini-2.0 batch measured and
published (`8104b26` + cache-size fix `8605806`) — the second plain-transformer MoE
(`bailingmoe2`, verified in llama-arch.cpp; the rollback list's BAILINGMOE3 is a
different arch) and a NON-THINKING model, at 9.9 GB the barely-fits edge case:
(a) 12.59 tok/s / 0.86 majflt (essentially resident), (b) 8.63 tok/s — streaming LOSES
31% and its 7.5 GiB expert cache + anon dense copy PUSHED THE MODEL INTO SWAP
(0.86 → 19.27 majflt/tok: the cache became the memory pressure; sharpest fits-RAM
result yet), (c) reuse native from the first follow-up, no echo, no reconcile turn;
auto-echo a verified no-op (c3 ≈ c2 + warm decode); c4 divergence 27/34 with rs-seq
OFF; c5 ≡ c4; warmup T1 ~10× (0.80 s) composing freely (1/31). One audit catch
(7.7 → 7.5 GiB) fixed same session. Session-8 (Laguna non-hybrid find) in history.
Queue: Qwen3-30B next.*## State delta (this session)

- **Arc pushed at session start (user request):** `8958e88..b7f3cd0`; the session-9
  wrap-up commit below is the only local commit again.
- **Ling-mini-2.0 full batch (`8104b26`, fix `8605806`) — second plain transformer,
  barely-fits edge case:** arch `bailingmoe2` (no hybrid/recurrent listing; the
  rollback list's BAILINGMOE3 is a different arch) and non-thinking (no reasoning
  span in replies). 9.9 GB on 11 GB RAM: (a) 12.59 tok/s, load 21.3 s, prefill 2.26 s,
  0.86 majflt/tok (essentially resident); (b) 8.63 tok/s (−31%), prefill 12.75 s
  (5.6×), 19.27 majflt/tok — the streaming stack's 7.5 GiB cache + anon dense copy
  pushed the model into swap: the cache BECAME the memory pressure (sharpest fits-RAM
  result yet, headline of the recommendations); (c) reuse native from the first
  follow-up (c1 T2 already 27/34) with no echo and no reconcile turn; `--auto-echo`
  a verified no-op (c3 ≈ c2, decode gains from warm cache only); (c4) divergence 27/34
  with rs-seq OFF (free chop); (c5) ≡ c4 (pool unused — same classification
  confirmation as Laguna); warmup T1 ~10× (0.80 s) composing freely (1 fresh / 31).
  c4's T1 prefill (34.4 s at 1.0 tok/s) noted in-doc as the cold-load outlier row.
- **MAXTOK for Ling-mini:** 192 suffices (non-thinking model, no reasoning span);
  c-suite ran in one tmux loop, all five cells inside ~5 min.
- **Audit catch this session:** cache 7686.8 MiB is 7.5 GiB, not 7.7 (fixed `8605806`).
- **Watched PRs (this session):** not re-checked; last verified OPEN in session 6.
- **Session 8 record (arc push, Laguna non-hybrid find, MAXTOK 384 ladder, MTP-carrier
  audit fix):** in the session-8 history entry below.

## Artifacts touched (this session)

| File | What |
|---|---|
| `scripts/host-bench-ling-c1c5.patch` | the Ling-mini rows + summary/README fold-in diff published as 8104b26 |
| `scripts/host-bench-ling-fix1.patch` | the cache-size fix published as 8605806 (7686.8 MiB = 7.5 GiB) |
| `PROGRESS.md` | this rewrite + the session-9 history entry |
| `cjl4hd:main` `8104b26` + `8605806` | published: Ling-mini section, summary/recommendations updates, README perf column |
| `.bench-report/Ling-mini-a-baseline.{csv,log}` | Ling-mini cell (a) raw evidence; bench-report's own CSV covers cell (b) |

Evidence (ephemeral, regenerable by rerunning the cells): `/tmp/bench-ling/server-c{1..5}.log`,
`/tmp/bench-ling/c{1..5}/` (per-turn t*/r* JSON); earlier sessions' `/tmp/bench-{cyber,lfm25,laguna}/`
trees likewise still on disk.

Arc state: `feat/session-residency` == `fork/feat/session-residency` at `b7f3cd0`
(pushed this session); this wrap-up commit goes on top locally per the session-5
convention. The `core/src/engine/session.cpp` pos0 port stays working-tree-only —
NEVER commit it (the pin still has `n_past` and would not build); stash it for pin
builds and the ctest gate. Engine-side branches: `bench/host-rs` on cjl4hd/llama.cpp
(what `build-bench/` links); `fix/rs-rollback-index-shift` upstream, PR #29117 closed
until #29085 merges.

## Environment state

- **Server**: DOWN (the c5 OOMs took it; restart when a serve/agents session needs it).
  LFM2.5 daily driver: `setsid nohup python3 -u scripts/bmoe-serve.py -m ~/llm/models/LFM2.5-8B-A1B-UD-Q4_K_M.gguf --engine-args "--ctx-size 8192 --chatml" --auto-echo --port 8017 > /tmp/bmoe-serve.log 2>&1 &`
  (Ling-mini alternative in the history; `--auto-echo` only for thinking models; run it
  inside tmux — background processes die between tool calls here).
- **Models** (`~/llm/models/`): Ling-mini-2.0, LFM2.5-8B-A1B-UD-Q4_K_M (note: no plain
  `-Q4_K_M` file — sweeps use the UD file), Qwen3.5-9B, olmoe-1b-7b, Laguna-XS-2.1,
  Ornith-1.5, Qwen3-30B, Qwen3.6-35B, Cyber-Tiel-35B.
- **Warmup cache** `~/.cache/bmoe-serve/warmup.json`: holds the Ling-mini chain (valid,
  self-regenerating — the next serve on any other model overwrites it).
- **Bench build**: `build-bench/` = the arc linked against the clone llama
  (`bench/host-rs`); requires the session.cpp pos0 working-tree port. The pin build
  (`build/`) must not see that port.
- **Remotes**: `origin` = Helldez/BigMoeOnEdge (upstream; PR #197 from fork's
  `feat/serve-bridge-arm64`), `fork` = cjl4hd/BigMoeOnEdge (push target); `gh` authed
  as `cjl4hd`. Submodule: `Helldez/llama.cpp` @ `0e8c83e51` (one sanctioned expert-hook
  commit on upstream).
- **llama.cpp work area**: `~/git/llama.cpp` — fork `cjl4hd/llama.cpp` (origin),
  `upstream` = ggml-org, `helldez` = pin archaeology. Release build with fixture models;
  regenerate via `cmake --build build -j4 --target test-llama-archs &&
  ./build/bin/test-llama-archs -o build/tests/test-models/`.
- **aider scratch repo**: `~/aider-test` (planted `a - b` bug in `calculator.py`).
- Untracked, NOT ours: `.opencode/`, `bmoe-arm64*`, `opencode.json`, `.aider*`, logs.
- Ephemeral: `/tmp/bmoe-serve.log`, `/tmp/bmoe-reqs.jsonl` (only when `BMOE_DEBUG_ECHO=1`),
  `/tmp/bf-*` bench outputs, `/tmp/sweep-*.txt` + `/tmp/rsbench-*.err` (this session's
  matrix evidence, all regenerable via the rsbench commands in the history entry below).

## Open questions / blocked items

1. **Stacked PR held until #197 merges** (user decision). Plan: verify ancestry → sync
   fork main → rebase `feat/session-residency` (serve-bridge commits collapse) →
   `gh pr create --repo Helldez/BigMoeOnEdge --base main --head cjl4hd:feat/session-residency`.
   Fallback if #197 stalls: fork-internal PR (`--repo cjl4hd --base feat/serve-bridge-arm64`), retarget later.
2. **PR #29085 (reserve fix) awaits upstream CI/review** (checked this session: OPEN,
   REVIEW_REQUIRED). When merged it reaches this dependency only via a submodule bump —
   re-run the byte-identity gates after the bump (ADR-001's bump rule), and re-run
   `bmoe-rsbench reserve` on the new pin (the backtrace site differs pin↔master).
3. **Shape-dependent-backend caveat for any future bitwise claim:** any exactness
   comparison against a differently-shaped reference is meaningless here (~3 logits of
   noise from ubatch splits alone, MoE routing flips). Only identical-shape controls
   (d=0) or argmax-level verdicts with shape-control rows are admissible evidence.

## Next actions (ordered)

1. **Continue the bench batch — Qwen3-30B-A3B next** (Ling-mini done), then
   Qwen3.6-35B, olmoe. Per model: (a) direct pin-build CLI run — NOT
   `bench-report.sh`, which hardcodes the streaming stack and is cell (b) — same
   protocol minus the streaming flags; (b) `scripts/bench-report.sh`; (c1–c3)
   `cellc.sh` (MAXTOK: 160 for the 35Bs, 192 for non-thinking/small models, 384 for
   reasoning-heavy Laguna-class — start 192 and ladder from the r1/r2 reasoning
   lengths); (c4/c5) divergence cells — c5 budget stays 64 per the OOM law; run the
   c-suite in ONE detached tmux loop (survives tool calls; the loop self-terminates,
   results live on disk). Publish each model's rows via
   `scripts/publish-host-bench.sh`, then fold the model into the doc's
   Summary/Conclusions/Recommendations and the README perf column. **Audit rule
   (five catches now):** every number, ratio, and unit conversion (MiB→GiB!) from
   the run's own CSV/log — never from memory. Dense models skipped per user.
2. **Check watched PRs** (`gh pr view 29085 --repo ggml-org/llama.cpp` and #197 on
   Helldez/BigMoeOnEdge): last verified OPEN in session 6 — three sessions stale.
   When #29085 merges: reopen PR #29117 (`gh pr reopen 29117 --repo ggml-org/llama.cpp`,
   fall back to re-creating from branch `fix/rs-rollback-index-shift`), humanize its
   description first (`/tmp/pr-index-shift-description-draft.md` is the tool draft — a
   starting point only), then ready-for-review. Flag in the PR: `seq_rm` now returns
   false where it used to return true; callers that ignore it fall back to re-prefill.
   Watch item: #28976 (WebGPU GDN kernel) must keep the snapshot-slot contract. When
   #197 merges: execute the stacked-PR plan (Open questions 1); submodule bump + full
   gates when #29085 lands in a pin bump.
3. **After the batch: refresh the evidence tables** — `docs/benchmarks.md`/`docs/serve.md`
   gain host rows; the README in-flight table's perf column gets second/third points.
4. **Watch #29085 and #197** (`gh pr view 29085 --repo ggml-org/llama.cpp`); execute the
   stacked-PR plan when #197 merges; submodule bump + full gates when #29085 merges;
   plan the `--rs-seq` flip + hybrid-residency un-exclusion once a release carries the fix.
5. **Measure Ling-mini edit-turn reuse** with captured aider payloads; **opencode
   re-test** with `--auto-echo` on LFM2.5. Daily driver back on :8017 when benching ends.
6. **Every wrap-up: refresh the README feature tables** on `fork/main` (In-flight
   features + Forks; rule 6 lives there) — add/remove rows when branches merge or
   fork divergence changes.

## Resume gates (all must assert positives)

1. Pin build + tests (the session.cpp pos0 port breaks the pin build — stash first):
   `git stash push -- core/src/engine/session.cpp` → `cmake --build build -j4 2>&1 |
   grep -E 'error|warning' | grep -v 'ccache not found'` → empty (ccache is simply not
   installed here; that advisory is environmental, not a build diagnostic) →
   `cd build && ctest --output-on-failure` → **13/13 passed** → `git stash pop`.
   Verify the port is back (`git diff --stat`).
2. `git status -sb` → `feat/session-residency` pushed to fork (or exactly the current
   wrap-up ahead of it); the ONLY dirty file is ` M core/src/engine/session.cpp` (the
   pos0 port — expected, never commit). Anything else dirty: triage before working.
3. `bash -n scripts/cellc.sh && bash -n scripts/publish-host-bench.sh && python3 -m
   py_compile scripts/bmoe-serve.py` → silent.
4. `git fetch fork -q && git merge-base --is-ancestor origin/main fork/main && echo synced`
   → prints `synced` (fork/main not behind upstream — the publish flow depends on it).
5. `test -x build/tools/bmoe-rsbench` → exists (`-DBMOE_BUILD_TOOLS=ON`); and
   `./build/tools/bmoe-rsbench reserve <lfm2 gguf>` → exit 134 on the unfixed pin
   (flips to 0 after the #29085 bump).
6. `cd ~/git/llama.cpp && git branch --show-current` → `fix/rs-rollback-index-shift`,
   clean tree, in sync with origin; re-verify cutsweep with `/tmp/bmoe-rsbench-clone
   cutsweep <lfm2 gguf>` → 9/9 EXACT (rebuild runner + clone llama if the branch moved).
7. Server gate (only when a serve/agents session needs it): restore per Environment
   state, then `curl -fsS -m 3 http://127.0.0.1:8017/v1/models` → the `bmoe-local` JSON.

If a gate fails: re-derive from artifacts (git log, docs/adr, history below) before
continuing. Never weaken a gate to make it pass.

---

# History — append-only evidence log

Newer entries at the bottom; never rewrite an entry — falsify explicitly.

## 2026-09-16 → 09-17 — Hybrid residency arc, phase 1: warmup + append reuse

Branch `feat/session-residency` (stacked on `feat/serve-bridge-arm64`, confirmed
ancestor). Goal: kill the full-clear-every-turn cost on hybrid models without breaking
recurrent-state validity.

**Shipped:**
- `2cb9cd3` — warmup replay of the conversation prefix at server startup (front-loads
  prefill while the user types). Tag `progress/2026-09-17-residency-warmup`.
- `e930b4d` — append-only prefix reuse for hybrids: when the next render strictly
  extends the resident token mirror, skip the hybrid clear and prefill only the delta.
  Every fail/cancel/overflow path poisons the mirror so the next turn full-clears.
  Pure-transformer behavior bit-identical (gates green).

**Key discovery (debug instrumentation, later removed):** on LFM2.5 the mirror holds
`[prompt][reasoning][answer]` but the client echoes only `[answer]` — a thinking
hybrid's re-render *can never* extend the mirror. Structural, not a bug; documented in
`docs/serve.md`. The engine's divergence fallback (full clear) handled it correctly.

## 2026-09-17 — Phase 2: snapshot rollback wired, proven non-bit-exact, shipped off

- `63768e7` — `--rs-seq N`: allocate upstream's `[EXPERIMENTAL]` per-token
  recurrent-state snapshots (`n_rs_seq`) so `seq_rm` mid-sequence becomes restorable
  within budget; the generic diff path already falls back to full-clear on `seq_rm`
  failure. Tag `progress/2026-09-17-snapshot-rollback`.
- Verified on Qwen3.5-9B (dense hybrid, arch `qwen35`, `think_ctl: template`):
  reuse engages (`n_reused` 21/33/58) **but restore is not bit-exact** — greedy decoding,
  identical prompt: `40` fresh vs `420` restored (and a self-correcting "wait, that's not
  right…" on a cache-warm repeat). Attention side exact; gated-delta-net snapshots are
  the divergence. **Default flipped to 0** (off) after proving the off-behavior
  byte-identical to the shipped baseline. See `docs/adr/001-hybrid-session-reuse-policy.md`.
- `5e664bf` — LFM2/LFM2MOE crash at graph reserve with snapshots enabled:
  `not enough space in the context's memory pool (needed 836640, available 836272)` —
  fixed ~368-byte shortfall, invariant to ctx (4096/2048), ubatch (256/full), budget.
  Upstream reserve under-counts snapshot ops; no public knob. Third upstream gate.
- Bridge: per-request `think` flag threaded (HTTP → engine). Template ground truth via
  jinja: Qwen3.5 silences thinking by baking an empty `<think></think>` into the
  generation prompt that re-rendered history omits — renders never nest, append can
  never fire on it even with thinking off.

## 2026-09-18 — Phase 3: reasoning echo — the rewind question answered

User question: "why can't we rewind the KV cache to before the last thinking stage?"
Answer built up in-session: (1) on transformers the diff path already reuses exactly to
that boundary; (2) KV surgery between reasoning and answer is impossible (positions
baked in, causal entanglement — only suffix chops are legal, and they drop the answer
too); (3) the better move is not rewinding at all.

**Ground truth from the LFM2.5 template** (gguf + jinja2, `{% generation %}` tags
stripped for stock jinja): a `preserve_thinking` variable (default false) renders
history reasoning verbatim — the echo mechanism exists natively. With it true,
`render1 + generated` is a strict prefix of `render2` (case A verified). Default kwargs
stop reuse exactly at the rewind-to-before-thinking boundary (123/177 tokens) — theory
confirmed empirically.

**Shipped `d83d153`** — `preserve_reasoning` request flag end-to-end:
`GenerateRequest` (`core/include/bmoe/session.h`) → `SessionCmd` (`cli/main.cpp`) →
`chat_template_kwargs["preserve_thinking"]` (`core/src/engine/session.cpp`) → bridge
(`scripts/bmoe-serve.py`, both handlers). Measured on LFM2.5-8B-A1B, 3-turn echo chain,
answers `4`/`5`/`6` all correct:

| Turn | prefilled | reused |
|---|---|---|
| T1 | 28 | 0 |
| T2 | 17 | 101 |
| T3 | 17 | 235 |

No-echo control: full clear every turn (n_reused=0), as designed. Decode measured
~9.2 tok/s in the resident regime — fastest LFM2.5 result of the project. See
`docs/adr/002-reasoning-echo.md`. Docs updated: `docs/serve.md` (gates + echo section),
`docs/telemetry.md` (protocol field), `CHANGELOG.md`.

**Upstream check** (same day): #25913 still open (touched 09-12); rollback allowlist now
includes LFM2/LFM2MOE (intent confirmed, reserve crash blocks it); no bit-exactness fix
landed. `--rs-seq` stays off.

## 2026-09-18 — Session wrap-up: lifecycle docs + ADRs

Gates re-run and green: clean build, 13/13 ctest, server healthy on Ling-mini :8017,
branch in sync with fork. Decision sweep produced `docs/adr/001` (hybrid reuse policy)
and `docs/adr/002` (reasoning echo), indexed in `docs/README.md`.

Resume doc (`SESSION_SUMMARY.md`) written with next actions: stacked PR (fork-internal base),
bridge auto-echo prototype, aider benchmark on LFM2.5.

*2026-09-18 note: renamed per the updated lifecycle skill — this file was RE_PROGRESS.md,
the resume doc became PROGRESS.md (formerly SESSION_SUMMARY.md); consolidated back into a
single PROGRESS.md later the same day per user choice.*

## 2026-09-18 — Feature-flag audit, bench-features.sh, lifecycle migration

User question: "are all of our new features behind command line flags?" — answered by
audit: no, and deliberately. Warmup is a bridge default (`--no-warmup`), echo/think are
per-request fields; `--rs-seq` is the only new CLI flag (off by default, ADR-001).

**Shipped `scripts/bench-features.sh`** — before/after proof of all three residency
features over the bridge, isolated port (default 8019), 3-turn chain with VERIFIED
answers (42/52/62; fast-but-wrong = FAIL). Debugging it exercised real failure modes:
(1) a static "template mentions think" probe over-triggered on Ling-mini — replaced with
a behavioral probe on `reasoning_content`; (2) forgetting `--chatml` in the bridge args
reproduced the known empty-prompt 502 from earlier sessions — engine args now default to
`--chatml` (overridable via `EXTRA_ENGINE_ARGS`); (3) `gguf-py`'s `get_string` silently
fails on this version — the fields API is the working one; (4) chain construction must
be incremental (each turn needs the prior reply file); (5) answer verification must
match the exact content field (`"content": "52"`), not a substring (52 ⊂ 525); (6) the
script backs up/restores the global `warmup.json` so bench runs never contaminate the
user's warm cache.

Measured (Ling-mini-2.0, this host): warmup off→on cuts T1 prefill 2.19 s → 0.06 s
(~35×), tok/s 9.4 → 16.1; echo skipped behaviorally; `--rs-seq` a verified no-op on pure
transformers. Table in `docs/serve.md`; CHANGELOG updated.

**Lifecycle migration** (user choice): single-`PROGRESS.md` convention (resume section
rewritten per session on top, append-only history below) replaces the two-file scheme.
`SESSION_SUMMARY.md` → `PROGRESS.md`, `RE_PROGRESS.md` → `PROGRESS.history.md` (git mv,
history preserved). The project-lifecycle skill was updated in both home copies —
discovery: `~/.agents/skills/` was STALE (missing the SOFTWARE_REQUIREMENTS module);
`~/skills/skills/` was newer and is now the synced source of truth.

**Stacked-PR plan recorded and HELD** (user decision: wait for serve-bridge PR #197 to
merge): after merge, sync fork main → rebase `feat/session-residency` onto it (the
serve-bridge commits collapse into main) → `gh pr create --repo Helldez/BigMoeOnEdge
--base main`. Fallback documented in PROGRESS.md Open questions if #197 stalls.

## 2026-09-18 (evening) — LFM2.5 matrix, --auto-echo shipped and proven

Followups executed: PR #197 still OPEN (stacked PR remains held). The LFM2.5 bench run
surfaced three script bugs, each fixed in the script: (1) static "template mentions
think" probe over-triggered — echo skip is behavioral on reasoning_content; (2) the
INT/TERM trap returned instead of exiting, so a killed script RESUMED into the next
scenario as a zombie (cost: it later overwrote the restored warmup.json — user's cached
LFM2.5 math chain lost; self-regenerates, lesson recorded); (3) the 6*7→+10→+10 chain
relied on coreference ("add 10 again") which the 1B model parsed as "repeat 52" —
questions now restate their inputs (6*7=42, 42+10=52, 52+10=62), testing the cache, not
the parser. Also: thinking models need N_PREDICT≈192 (reasoning alone eats smaller
budgets — nothing emitted, verification fails regardless of cache behavior).

**LFM2.5 matrix, all verified:** echo-off T2/T3 prefill 2.46/3.59 s (n_reused 0) vs
echo-on 1.38/1.27 s (n_reused 124/205); warmup rows equal (stale warmup file, mechanism
proven on Ling-mini); rs-seq-on reproduces the lfm2moe graph-reserve crash at load.

**--auto-echo shipped (ADR-003):** bridge records each reply's exact
(reasoning, answer) span (cap 16, newest-wins) and rewrites matching assistant history
turns before the engine sees them. First live test FAILED (n_reused=0) — the rewritten
spans were stripped by the template unless preserve_thinking is set; auto-echo now
implies preserve_reasoning. Retest: plain client echoing ONLY answer text got
n_reused 124 → 205, prefill ~24 tokens/turn, answers 42/52/\boxed{62} all correct.
Unmodified OpenAI clients now get delta-only prefill on thinking hybrids.

Process notes for future sessions: never `pkill -f` a pattern that matches the invoking
shell's own command line (killed our own launches twice); after killing a bridge, WAIT
for the port to actually free (ss -tln) before rebinding — the bind races the dying
process's TIME_WAIT and the new bridge dies after loading the engine.

## 2026-09-18 (night) — aider debugging: two client behaviors found, one fixed bridge-side

The aider live test showed n_reused=0 on every turn despite --auto-echo. BMOE_DEBUG_ECHO
payload capture (/tmp/bmoe-reqs.jsonl, dump of pre-canonical + canonical arrays) exposed
TWO aider behaviors, not one:

1. **Shrinking newest turn** (FIXED bridge-side): aider appends its edit-format
   boilerplate ("To suggest changes to a file you MUST return…") to the NEWEST user turn
   only, so the same turn's payload shrinks in the next request. First fix attempt
   (strip boilerplate from all-but-newest) was WRONG — the same turn renders differently
   across requests. Correct fix: strip from ALL user turns and relocate the boilerplate
   into the system prompt once (request-independent canonical form). Verified: canonical
   msgs byte-identical across captured requests, no duplicate scaffold on later requests.
2. **Edit-turn file re-add** (inherent, not fixable): after an accepted edit, aider
   re-adds the file with NEW content at a NEW position ("I updated the files." + re-add
   block). Semantically required (model must see the updated file) and structurally
   unavoidable for a hybrid (cannot rewind over the changed span). Measured directly by
   replaying captured payloads: edit turn prefilled 955 / reused 0 (42.6 s); a
   pure-question follow-up prefilled 26 / reused 1683 (1.6 s).

Practical guidance: with aider + LFM2.5 + --auto-echo, batch edits; non-edit follow-up
questions are now ~25x cheaper prefill. ADR-003 addendum, docs/serve.md, CHANGELOG
updated. The debug capture stays behind BMOE_DEBUG_ECHO (off by default).

## 2026-09-18 (night) — aider telemetry confirmed; blockers routed (ADR-004); lifecycle consolidated

Mixed aider session confirmed the ladder on LFM2.5: edit turns 426/815 prefilled with
0 reused (full clear, ~18/36 s); non-edit turns **18/16 prefilled with 587/1243 reused,
~0.8 s prefill**. User asked "why can't we rewind before the edit?" — answered: that is
the generic diff path (free on transformers, unmeasured on Ling-mini), blocked on
hybrids by the two upstream defects; bounded payoff (~half of edit-turn prefill; the
changed file content can never be reused).

User asked "why wait on upstream — can't we make these changes?" — answer recorded in
**ADR-004**: the submodule remote is `Helldez/llama.cpp` carrying one sanctioned
1-commit expert hook, so fork patches are possible but governed. Decisions: (1) the
lfm2moe graph-reserve fix goes as an **upstream PR to ggml-org/llama.cpp** (not
Helldez/llama.cpp — that is only the submodule mirror); (2) bit-exact restore is NOT
attempted now; any future fork experiment would be a 1-commit branch on
Helldez/llama.cpp with maintainer agreement, never a new personal fork. Trigger to
revisit: upstream chunk-boundary snapshots or equivalent.

**Consolidation (user choice):** `PROGRESS.history.md` merged back into `PROGRESS.md`
(resume section + history in one file); `PROGRESS.history.md` removed. ADR-004 indexed
in docs/README.md.

## 2026-09-18 (late) — Blockers phase: reserve root-cause + upstream PR; exactness baselines

User decision: attempt ALL blockers including bit-exact restore; route the reserve fix as
a mainline contribution; the pin stays untouched and bumpable throughout (work happens in a
separate clone, never in `third_party/`).

**Setup.** `ggml-org/llama.cpp` forked to `cjl4hd/llama.cpp`; clone at `~/git/llama.cpp`
(origin=cjl4hd, upstream=ggml-org, `helldez` remote added for pin archaeology). Release
build with tests; rollback-suite fixture models generated via
`cmake --build build -j4 --target test-llama-archs && ./build/bin/test-llama-archs -o build/tests/test-models/`.

**Repro A — the reserve crash.** First probe (upstream-default shapes: ubatch 512,
n_rs_seq 8) did **not** crash — the shortfall is graph-shape dependent. Mirroring the
engine's exact context shape (n_ctx = n_batch = n_ubatch = 2048, `use_extra_bufts=false`,
n_rs_seq 64) reproduced it byte-for-byte on pristine master `4fea119de`:
`needed 836640, available 836272`, `GGML_ASSERT(obj_new)` — the same numbers as on our pin,
proving it is not a pin artifact.

**Root cause.** `llama_context::graph_max_nodes()` buckets node budgets by arch; the
linear-attention family gets `max(n_tokens*40, 32*n_tensors)` because GDN graphs with
snapshot planes need the headroom. LFM2/LFM2MOE are on the rollback allowlist
(`llm_arch_supports_rs_rollback`) but missing from that bucket → default
`max(1024, 8*n_tensors)`, which their snapshot graph exceeds by exactly one `ggml_tensor`
object (368 B = GGML_OBJECT_SIZE + GGML_TENSOR_SIZE). Invariant to ctx/ubatch/budget
because the budget itself scales with those.

**Fix and PR.** Two lines (add the archs to the bucket) plus registration of the `lfm2` /
`lfm2moe` fixture models for `test-recurrent-state-rollback` — the generator already
produced them, but no rollback test had ever exercised either arch. Suite passes 7/7
(qwen35, nemotron-h, dsv4, kimi-k3 + new lfm2, lfm2moe). Committed `74e1ee6de`, pushed to
the fork, **PR opened: ggml-org/llama.cpp#29085**. Note: an outside contributor cannot
push a branch to `Helldez/llama.cpp`; mainline PRs therefore go via the user's own fork.
The Helldez 1-commit fork-branch option stays reserved for anything that must bridge the
submodule pin before upstream merges (needs maintainer agreement, AGENTS.md #1).

**Repro B — exactness baselines (phase 2 open).** New `diverge` mode: fresh greedy decode
vs greedy decode after a snapshot rollback, against an untouched reference context.
- Qwen3.5-9B: DIFFER at rollback depth 8 (restored stream began correct — `hello world` —
  then degenerated into `<|im_start|>` repetition).
- LFM2.5-8B: DIFFER at depths 8 and 3 (3 = the upstream fixture test's parity depth).
  Restored streams degenerate into prompt-tail echo loops.
- Position-dependence (sometimes-correct start) is consistent with the chunk-boundary
  hypothesis; the depth × ubatch matrix is the next artifact.
- (A fixture-model probe is invalid: synthetic test vocabs do not support
  `llama_tokenize` — the real models above are the evidence.)

**Tooling.** `tools/rsbench.cpp` → `bmoe-rsbench` (engine-shaped context, public API
only), the deliberate exception to the tools' no-llama rule, opt-in via
`-DBMOE_BUILD_TOOLS=ON`. A/B proof in one command: against our unfixed pin it aborts
(exit 134, backtrace through `ggml_view_3d` in the pin's older LFM2 graph); against the
patched clone it reserves cleanly. Pin/master backtrace call sites differ — re-verify the
repro after every submodule bump.

## 2026-09-18 (late, session 2) — exactness falsified as a harness bug; the m-law

Goal (next actions 2–3 of the previous resume): the depth × ubatch × snapshot matrix and
the chunk-boundary hypothesis test. Both executed — and the outcome falsified the premise.

**Source trace first** (pin @ `0e8c83e51`): GDN CPU kernel `ggml-cpu/ops.cpp:10752` is
strictly sequential per token and writes snapshot slot `n_tokens−1−t` per step (only
`min(n_seq_tokens, K)` planes per ubatch); conv writes in `delta-net-base.cpp:479` map
slot t → state `(n_seq_tokens−(ch−1)+t)…` clamped at 0; LFM2 shortconv (lfm2.cpp:211)
writes true per-token planes (guard `causal_attn` holds — LFM2 not in the non-causal list,
no `attention.causal` key in the gguf); `seq_rm` rollback needs depth ≤ n_rs_seq, is
single-use, and `rm_all` resets `rs_idx`; restore reads plane `rs_idx` via the
`s_copy` gather. During the trace: **`bmoe-rsbench diverge` was self-defeating** — its
continuation went through `greedy_generate`, which opened with a full `seq_rm(-1,-1)`,
wiping the pending rollback. Every prior exactness datum was that artifact.

**Rewritten tool** (`tools/rsbench.cpp`): `diverge` = one matched-feed cell (identical
token feeds both sides, pending rollback never cleared, reference continues instead of
re-clearing); `sweep` = mode {single, batched} × d_rm {1,3,8,24} × m {0,4} matrix, per-cell
predicted-vs-observed, plus a sensitivity probe (full-clear+re-prefill vs continuation)
that qualifies each prompt. Process lessons: stdout must be line-buffered (two 600 s
timeouts lost their buffered output before the fix); `pgrep -f` self-match false-positived
(the known trap from the evening entry); the counting-chain prompt is state-INSENSITIVE
(14/14 EXACT cells on Qwen3.5 including full-clear probes — EXACT there is uninformative);
the fox prompt is sensitive.

**The m-law (matched-feed, sensitive prompts, both families, pin and clone agree):**

- m=0: EXACT everywhere tested on qwen35 (d=1,3,8,24; single AND batched rm — the
  predicted single-mode conv collapse is falsified; the clamp yields oldest-window, which
  for slots 0/1 is correct) and on lfm2moe at d=1,3,24.
- m≥1: DIFFER everywhere, both families, all depths, both rm shapes. Mechanism:
  single-token steps refresh only plane 0; planes d≥1 keep last-multi-token-ubatch values
  (stale by m). This is the real edit-turn shape of the server.
- lfm2moe m=0 d=8: DIFFER identically in both rm shapes (`100` vs `99`) — residual anomaly,
  OPEN (next action 2).
- Upstream's fixture test passes because it never generates before the rollback (m=0 by
  construction) and (checkpoint path) both sides read the same planes; PR #29085's lfm2
  rows never exercised m≥1 either.

Evidence files (regenerable, ephemeral): `/tmp/sweep-q35.txt` (std matrix, insensitive
prompt), `/tmp/sweep-q35-fox.txt`-equivalent console output, `/tmp/sweep-lfm-std.txt`,
`/tmp/sweep-lfm-fox.txt`, `/tmp/reserve-pin.txt`; run via
`/tmp/bmoe-rsbench-clone sweep ~/llm/models/<model>.gguf [fox]` (clone-linked) or
`./build/tools/bmoe-rsbench` (pin-linked). Clone runner compile line in Artifacts above.

**Docs:** ADR-004 Addendum 2 written (falsification + law + candidate upstream fix:
ubatch-replay-before-restore or per-token plane maintenance); Addendum 1's exactness claim
struck; ADR-001 §Context 2 superseded; docs/serve.md and CHANGELOG 0.24.2 corrected.
Gates all green after the rewrite: clean build, 13/13 ctest, `reserve` still exit 134 on
the pin. No code-behavior change anywhere in the engine — the falsification changes
documentation and the upstream plan, not this repo's defaults (`--rs-seq` stays off for
the staleness reason now, not the non-exactness reason before).

## 2026-09-18 (late, session 3) — the mechanism mapped end to end; the index-shift fix

Goal (next action 1 of the previous resume): the staleness fix. Executed — but only after
the instrument itself was fixed one more time, which is where the session's real findings
live.

**Kernel-level plane law, final form** (GDN: `ggml-cpu/ops.cpp` `target_slot = n_tokens−1−t`,
slots ≥ n never written; conv: `lfm2.cpp` `n_written = min(n, K)`, `delta-net-base.cpp`
pre-fix clamped ALL K slots every ubatch; `s_copy` maps `rs_idx` 1:1 to plane rows): a
ubatch of n tokens writes planes 0..min(n,K)−1, plane p = state p tokens before its end;
single-token steps rewrite only plane 0. `seq_rm` reads plane d. Consequence: exact iff
the wanted state still occupies plane d — m=0 cuts into the last multi-token ubatch only.
The **d=8 anomaly is resolved**: the sweep's rm ubatch had exactly d tokens → plane d was
never written by it → garbage read, arch-independent (not an LFM quirk).

**The shape-noise confound (the session's decisive discovery).** A no-rollback control
(same tokens, 10-token prefill vs 6+4 split) moves logits by up to **3.6** on this backend
(MoE routing flips amplify accumulation-order noise). This invalidated: (a) upstream's own
multi-seq fixture, which FAILS on vanilla master (diff 11.6) comparing 12-token-ubatch vs
10-token-ubatch histories at eps=1e-7 — a bar nothing can meet; (b) every
`statecmp`/`dsteps` "restore is not bitwise" result from earlier this session (only the
d=0 identical-shape control was sound, and it passed). Lesson recorded as an open-questions
rule: bitwise comparisons need identical ubatch shapes; everything else is argmax + shape
controls.

**The fix** (`~/git/llama.cpp`, branch `fix/rs-rollback-index-shift` off origin/master
4fea119de): per-seq epoch bookkeeping (`rs_epoch_end` / `rs_epoch_planes` / `rs_epoch_lo`)
set in `find_slot` per multi-token ubatch; `seq_rm` restores plane `d−m` when the wanted
state survives, refuses honestly otherwise (destroyed planes; checkpoint-loaded state — a
state blob carries a single plane, so nothing exists to roll back into; cleared seqs;
pending rollback); `delta-net-base.cpp` conv writes aligned to min(n,K) (was: clamped all
K — desynced conv from GDN planes after single-token steps); `prepare` dry-run saves/
restores the bookkeeping; `rm_all`, tail invalidation and fresh starts reset it;
`seq_cp` inherits, `seq_add` follows affine shifts, `seq_div` invalidates. Code review
caught and fixed: discarded-timeline planes after rollback+single-token replay (the
`rs_epoch_lo` floor), stale epochs surviving sequence teardown, fixture's dst-side
impossible rollback (now asserts the refusal).

**Verification** (`cutsweep`, new rsbench mode: c tokens cut into the prefill × m
single-token steps, each cell with a no-rollback SHAPE-control row — the rescuable shape,
unlike `sweep`'s unrescuable ones):

- fix build: **9/9 EXACT** on lfm2moe AND qwen35 (controls SAME everywhere)
- vanilla: 3/9 EXACT (m=0), 4 DIFFER (m≥1), 2 REFUSED (c+m > n_rs_seq)
- fixture test passes on both cache fills: single-seq bitwise incl. checkpoint round-trips
  and dirty-context load; multi-seq shape-gated (probe in-test, asserts mechanics + refusal
  semantics, reports diffs on shape-dependent backends)

Tool notes: `cutsweep` uses `n_rs_seq=8` because LFM2 archs abort at graph reserve with
larger values until #29085 lands (runner must not depend on that PR). `predict()` and the
sweep's top-of-file comment now document vanilla's behavior; `sweep` cells against the fix
build report REFUSED/INFRA, which is the honest verdict for those shapes.

Evidence: `/tmp/cutsweep-{fix,vanilla}-{lfm,q35}.txt` (vanilla baseline via stash push/
pop around a rebuild — regenerable; see Artifacts). Upstream branch pushed to
`cjl4hd/llama.cpp` and opened as ggml-org PR #29117 (draft); PR description draft at
`/tmp/pr-index-shift-description-draft.md`.

Session 3 addendum (2026-09-19): the root-README feature tables (`In-flight features` +
`Forks`) and the AGENTS.md rule-6 wrap-up mandate landed on **`cjl4hd:main`** (`b1f34f7`),
not on this branch — the arc stays free of them and the tables document unreleased work
from the canonical repo's point of view. An intermediate commit carrying them
(`cc8a999`) was force-pushed off this branch at the user's request. Rule 6 lives on
`fork/main` for now; this branch picks it up at the next main→arc sync (or the eventual
stacked PR does).

## 2026-09-19 - Session 4: host feature-bench campaign; Ornith 1.5 done

User: benchmark ALL on-disk models, three cells each - (a) mmap baseline, (b) bmoe streaming
stack, (c) bmoe + llama-side features - results on `cjl4hd:main` linked from the in-flight
table. Scope per user: distinct archs (dense R1-Distill/Qwen3.5-9B and quant variants
skipped), Ornith first then approve.

Harness facts (must not be re-derived): background processes die between tool calls here
(`process_type=BACKGROUND` unimplemented; setsid/nohup dies too) - every cell lives inside
ONE 575 s window; tmux sessions DO survive (the 8017 server now runs in tmux session
`bmoe-serve`). `bench/host-rs` on `cjl4hd/llama.cpp` = upstream `4fea119de` + index-shift
`7b2ec36d1` + reserve `8f3e6776b` (cherry-pick) + expert-ready hook `2a8d47ac9`
(cherry-pick; ONE conflict - upstream's IQP fast path in `mul_mat_id` - resolved
hook-first so both consumers gate on it); pushed. `build-bench/` = the arc built against
the clone (submodule detached at `2a8d47ac9`, pin restored after). `session.cpp` carries
a WORKING-TREE-ONLY port: upstream `4fea119de` renamed
`common_speculative_draft_params.n_past` to `pos0` (MTP-off path unaffected; NOT
committable - the pin still has `n_past` and would fail to build).

`scripts/cellc.sh` committed (arc): one scenario per invocation; verified 42/52/62 chain
over the bridge with word-boundary answer checks; scenarios c1 warmup-off (seeds the
warmup file), c2 warmup-on (replays), c3 `--auto-echo` + `preserve_reasoning` + echoed
chain; `MAXTOK` env (160 for the 35Bs). Warmup cache backed up on first call, restored
when c3 exits.

Ornith-1.5-35B-Q4_K_M (`qwen35moe`, 20.2 GB, 4-core host, 8.5 GiB avail RAM):
(a) baseline **1.39 tok/s**, load 145 s, **470 majflt/tok** (thrash); (b) streaming stack
**2.06 tok/s (+48%)**, load 86 s, cache hit 88.6%, 129 majflt/tok; (c) warmup T1 prefill
40.7 s -> **8.6 s (4.7x)**; auto-echo T3 **n_reused 98 / n_prompt 28**, prefill -50%,
tok/s **+69%**, answers verified. T2 stays a full clear: the FIRST echoed turn's render
must reconcile with what was generated (same qwen35 template mechanism as Qwen3.5-9B),
then later turns ride the cache. `--rs-seq` eligible (bench build) but off: Ornith's
echo-style reuse is template-blocked. Evidence: `/tmp/bench-ornith/*.{csv,log}`.

Results published: `docs/host-benchmarks.md` on `cjl4hd:main` (`8be5bc5`) with the README
residency row pointing at it; tooling caveat documented (bridge/cellc live on the arc
until #197 merges).

## 2026-09-19 (session 5) — c4/c5 divergence cells; the c5 OOM (three kernel kills)

Continued session 4's follow-ups: cellc.sh gained c4/c5 (divergence-turn cells: the
second request resends T1 + the engine's own reply and asks a NEW question — a hybrid
must rewind to just after A1, so rs-seq OFF full-clears (c4) and rs-seq ON is the
bounded rewind (c5); verify 42 then 62). Uncommitted alongside it: the session.cpp
pos0 port (working-tree-only, NOT committable — session 4's protocol note).

**The OOM (why sessions kept dying):** the kernel OOM-killer killed bmoe-cli three
times today (10:57, 12:32, 12:48; all ~10.4–10.5 GB RSS, all in bench-driven scopes).
Root cause is arithmetic: c5 launched with `--rs-seq 160` (a mid-session edit meant to
cover MAXTOK) → `llama_memory_recurrent: size = 10112.81 MiB ... S (f32): 9660.00 MiB`
on Ornith (~60.4 MiB per snapshot plane × 160) on an 11 GiB host. Load completes, the
first token work thrashes swap, OOM. Nothing in the engine is at fault: seq_rm
failures fall back to full clear everywhere (verified), and c4/c5 content generation
ran clean at normal RSS before each kill.

**c4 result (rs-seq OFF, completed pre-kill):** verdict ok (42/62 verified); T1
n_prompt 24 / n_reused 0; T2 divergence n_prompt 56 / n_reused 0 — the designed
full-clear baseline. Evidence: `/tmp/bench-ornith/server-c4.log` + `c4/`.

**c5 result (rs-seq 64, rerun after resume with user approval):** verdict ok (42/62
verified). T1 n_prompt 24 / n_reused 0; T2 divergence n_prompt 33 / **n_reused 23** —
the snapshot rewind fired (restored to just after T1, re-prefilled only the
plain-rendered reply + question, ~10 fresh tokens) and the restored stream decoded 62
correctly with no degeneration: the index-shift fix's first live end-to-end proof
through the real engine on a 35B hybrid (cutsweep's 9/9, now live). Honest wall-clock
read: prefill_s 15.45 ≈ c4's 14.50 despite 23 fewer prompt tokens — on this IO-bound
streaming host cached-token compute is not the bottleneck, so c5's win is
mechanism-proving, not latency. No OOM: snapshot cache ≈3.9 GiB at 64 planes, peak
fit within the 9.3 GiB available. Evidence: `/tmp/bench-ornith/{server-c5.log,c5/}`.

**Cleanup done:** the user's global warmup.json restored from the cellc backup (a c5
attempt had clobbered it with a 196-byte file; leftover copy kept at
/tmp/warmup-c5-leftover.json); no stray listeners; cellc.sh + bmoe-serve.py syntax
gates pass. The session-5 commit (cellc.sh c4/c5 + this record) is local-only — the
push to `fork` is left for the user to call.

**Publish + wrap-up (same session, user request):** the Ornith c4/c5 rows are published
on `cjl4hd:main` `5988e17` — via the new `scripts/publish-host-bench.sh` (throwaway
worktree of fork/main → apply → drift guard vs origin/main → rule-7 identifying-data
scan → single commit → push HEAD:main), doc diff archived on the arc as
`scripts/host-bench-ornith-c4c5.patch`; landed tree verified with
`git diff origin/main fork/main --stat` (only intended docs). The published doc
corrects the Ornith `--rs-seq` bullet: echo-style reuse stays structurally blocked on
qwen35 templates, but the c5 rewind proves the snapshot-rollback path engages. The arc
carries cellc.sh c4/c5 + the publisher + this record (`20841ec` + the wrap-up commit);
the session.cpp pos0 port remains working-tree-only (stashed for pin builds + gates).

## 2026-09-19 (session 6) — Cyber-Tiel-Coder-35B full batch; the warmup mechanism resolved

Next action 2 of the session-5 resume executed for the queue head. Gate 1 re-run first
(stash pos0 port → pin build clean → 13/13 ctest → pop). Cell (b) ran first by accident —
`bench-report.sh` hardcodes `--moe-stream --cache-mb auto --io-threads 4 --overlap
--dense-weights anon`, which IS cell (b); cell (a) is the same protocol as a direct
pin-CLI run minus the streaming flags (recorded in the published protocol rows).

**Results (all verdicts ok, answers verified):** (a) 1.29 tok/s, load 47 s, 616
majflt/tok — thrash profile; (b) 2.19 tok/s (+70%), load 178 s, 72 majflt/tok, hit
81.9%, prefill 68.3 s — SLOWER than (a)'s 61.8 s, honestly so: prefill routes nearly
all experts so streaming has nothing to skip and pays the streamer's overhead; (c1–c3)
auto-echo T2 28 prompt / 222 reused, T3 28/265, prefill 42 → 10–12 s; (c4) divergence
full-clear T2 236/0; (c5) T2 REWOUND 33 prompt / 203 reused, 62 correct, no
degeneration — T2 prefill 42.6 → 19.3 s. Unlike IO-bound Ornith (c5 = mechanism proof,
no latency win), Cyber-Tiel's rewind pays WALL-CLOCK: 203 skipped tokens outweigh the
restore. And c5's T1 came back **1 prompt / 203 reused** (2.6 s vs ~45 s everywhere
else): warmup and rs-seq COMPOSE — the replay seeds what the rewind restores.

**Mechanism find (code-verified in session.cpp generate()):** the hybrid clear-block
runs BEFORE the residency diff — with `n_rs_seq==0` a hybrid may only APPEND to the
resident mirror; any non-append turn (warmup T1's short render, a divergence)
full-clears unconditionally, cells cannot be rewound without snapshots. This resolves
the c2 anomaly (replay completed — log "4/4 messages resident in 59s" — yet T1
n_reused 0): the warmup cells' zeros are the designed worst case, and what warmup buys
without snapshots is cold-start only (c2 T1 42.4 s vs c4's identical T1 47.6 s). With
snapshots the clear-block is skipped and the diff path is legal — hence c5. Corollary
recorded: a clobbered warmup.json costs nothing structurally (pre-snapshot, any
non-append turn cleared anyway); session 5's restore-after-clobber was precautionary.

**Publish:** `cjl4hd:main` `4418988` (rows + reading bullets + queue line), then a
correction `d19ead7` — the first patch carried cell (b) prefill 28.1 s, a pattern slip
from Ornith's sibling row; the run's own CSV says 68.255 s. Caught in the post-publish
audit against evidence. Rule going into Next action 1: never transcribe a published
number from memory or a sibling row — re-read the summary line of the run's own CSV.

**Tooling:** cellc.sh readiness wait 150 → 420 s (Cyber-Tiel's streamed load alone is
178 s; every c-cell on a 35B would have false-FAILed with SERVER-FAILED). Watched PRs
checked: #29085 OPEN/REVIEW_REQUIRED, #197 OPEN — held items stay held. Session commit
left local for the user to push (session-5 convention); `fork/main` carries the
published doc rows regardless.

## 2026-09-19 (session 7) — README perf column; summary/conclusions/recommendations; LFM2.5-8B batch

User request: refresh the README in-flight perf column with the Cyber-Tiel point; give
host-benchmarks.md a summary, conclusions, and recommendations; then repeat for LFM2.5.

**Doc work (`3999832`):** three README rows refreshed — append-reuse gained the
Cyber-Tiel host point; the `--rs-seq` row's stale "restore not bit-exact" claim
(falsified by the session-3 cutsweep and two live c5 proofs by then) replaced with the
true state; the warmup row qualified with the composition finding.
host-benchmarks.md gained a Summary / Conclusions / Recommendations section grounded
strictly in published rows, plus a protocol-line fix (192-token budget for thinking
models, ahead of the LFM2.5 cells that use it).

**LFM2.5-8B-A1B batch (`6d0bc1e`, `f9b9f4f`) — the fits-RAM contrast case** (5.0 GB on
11 GB RAM; lfm2moe): (a) 9.69 tok/s, load 10.5 s, prefill 1.65 s, 0 majflt/tok;
(b) 8.64 tok/s, prefill 7.9 s — streaming is a NET LOSS with nothing to fix: 0 faults
in both cells, so the (b) stack only pays overhead. This is the doc's third profile and
it sharpened the recommendations: streaming is for at-or-past-RAM models, not a default.
(c) warmup T1 7.59 → 1.12 s (6.8×) — on a resident model the replay is pure prefill
speed, no fault storm; reuse still 0 (prefix-cut, same designed mechanism). (c) auto-echo
T2 24 prompt / 124 reused, T3 24/205, prefill ~1.2 s — LFM2.5's template echoes reasoning
natively so reuse engages from the FIRST follow-up (no qwen35-style reconcile turn).
(c4) 48/0 full clear. (c5) rewind 26 prompt / 22 reused, prefill 2.82 → 1.25 s, answer
62 verified; T1 1 fresh / 22 reused at 0.12 s — warmup+rs-seq composition replicated on
a second arch family, and the rewind's wall-clock win confirmed on a model where the
skipped tokens are few but cheap.

**Second self-caught slip:** the fits-RAM recommendation first said "7× faster prefill";
7.9/1.65 is ~5×. Caught in the post-publish audit, fixed `f9b9f4f` same session. The
session-6 audit rule extended: derived ratios are audited against the CSVs too.

**State:** gates re-run green (pin build + 13/13 ctest, stash/pop around); no stray
listeners or engine processes; warmup cache holds the LFM2.5 chain; the arc is two
commits ahead of `fork/feat/session-residency` (session-6 and -7 wrap-ups, push left to
the user per convention); `fork/main` carries all doc commits (5988e17 → 4418988 →
d19ead7 → 3999832 → 6d0bc1e → f9b9f4f). Queue head: Laguna-XS-2.1.

## 2026-09-19 (session 8) — arc pushed; Laguna-XS batch: the first non-hybrid

User request: push the arc, then the Laguna-XS-2.1 batch and publish. Push done first
(`2070368..8958e88`), clearing the two-commit backlog.

**Classification find (the session's real result).** Laguna measured unlike every prior
model and the c-suite only made sense under one hypothesis: upstream classifies `laguna`
as a plain-transformer MoE. Verified three ways — `llama-arch.cpp` lists it in neither
`llm_arch_is_hybrid` nor `llm_arch_supports_rs_rollback`; the gguf carries only standard
attention keys (sliding window, rope dims) with no recurrent/conv state tensors; and the
c5 cell is behaviourally identical to c4 (snapshot pool unused). Consequences, all
published: the divergence turn reuses 55 tokens with rs-seq OFF (hybrids full-clear the
same shape — the divergence tax is a hybrid phenomenon); warmup composes freely (T1
1 fresh / 54 reused); `--auto-echo` is REDUNDANT on this template and its echo rewrite
actually costs a 252-fresh reconcile turn (c3 T2 44 s) before a decode-side T3 win —
recommendation: leave it off on laguna-class templates. Also the first model whose
PREFILL speeds up under streaming (47.5 → 34.1 s): a ~2×-RAM baseline thrashes prefill
too. An earlier header edit claimed Laguna was the MTP-capable arch — the post-publish
audit proved it backwards (Cyber-Tiel has `qwen35moe.nextn_predict_layers=1` + nextn
tensors; Laguna has zero) and `8c77846` restored the original carrier claim plus exact
fault ratios (3.6×/8.6×/9.9×, not "~6–9×").

**Process:** MAXTOK ladder measured — 192 and 256 truncate Laguna's think spans (coherent
verbose reasoning cut at the budget; verdict FAIL, empty content), 384 passes all cells;
the CLI has no reasoning-budget wiring (upstream's common/reasoning-budget.h unwired in
bmoe-cli), so max_tokens is the honest lever. All five cells ran in ONE detached tmux
loop (~25 min wall) since background processes die between tool calls; the loop self-
terminated and every result was re-derived from the on-disk logs (per-turn JSONs +
TELEMETRY lines), answers 42/52/62 verified in every cell.

**State:** no stray listeners/processes; user's tmux session untouched; warmup cache
holds the Laguna chain; arc synced at 8958e88 (wrap-up commit local on top);
`fork/main` doc chain now 5988e17 → 4418988 → d19ead7 → 3999832 → 6d0bc1e → f9b9f4f →
d336133 → 8c77846. Queue head: Ling-mini-2.0.

## 2026-09-19 (session 9) — Ling-mini-2.0 batch: second non-hybrid, the barely-fits edge

User request: push the session-8 wrap-up, then the Ling-mini-2.0 batch and publish.
Push done first (`8958e88..b7f3cd0`).

**Classification (verified before the c-suite ran, the session-8 lesson applied):**
`bailingmoe2` is in neither the hybrid nor the recurrent lists; the one rollback-list
hit in the grep was BAILINGMOE3 — a different arch. Second plain-transformer MoE in the
matrix, and a NON-THINKING model (no reasoning span in replies — the session-2
behavioral-probe finding). All c-suite differences followed: reuse native from the
first follow-up (c1 T2 27 prompt / 34 reused, no echo, no reconcile turn), `--auto-echo`
a verified no-op (c3 ≈ c2 with warm-cache decode gains), divergence turns free partial
chops with rs-seq OFF (c4 27/34), c5 ≡ c4 (snapshot pool unused), warmup composing
freely (T1 1 fresh / 31 reused; ~10× on prefill seconds, 0.08–0.8 s across cells).

**The barely-fits edge case (the batch's headline).** At 9.9 GB on 11 GB RAM the mmap
baseline is essentially resident: 12.59 tok/s, 0.86 majflt/tok, load 21.3 s. The (b)
streaming stack LOSES 31% decode (8.63 tok/s) with prefill 5.6× slower (12.75 s) —
because its 7.5 GiB expert cache plus the anon dense copy push the model into swap
(0.86 → 19.27 majflt/tok): the cache itself became the memory pressure. Folded into
the recommendations as the second and sharpest fits-RAM data point (LFM2.5: −11%;
Ling-mini: −31%). One audit catch: 7686.8 MiB is 7.5 GiB, not 7.7 — fixed `8605806`
same session (unit conversions joined the audit rule).

**Process:** c4's T1 prefill row (34.4 s at 1.0 tok/s) is the cold-load cell racing the
prior cells' page cache — noted as the outlier in-doc with its comparable T2 figure.
All five cells ran in ONE tmux loop in ~5 min (fast model); loop self-terminated;
results re-derived from on-disk logs, answers 42/52/62 verified everywhere.

**State:** no stray listeners/processes/tmux; user's tmux session untouched; warmup
cache holds the Ling-mini chain; arc synced at b7f3cd0 (wrap-up commit local on top);
`fork/main` doc chain through 8605806. Queue head: Qwen3-30B-A3B.
