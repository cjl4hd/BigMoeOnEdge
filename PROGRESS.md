# PROGRESS — session entry point + evidence log

**Structure (per the project-lifecycle skill):** the resume section below is rewritten
every session — never append to it. Under the divider, the append-only history log holds
the long-form evidence narrative; entries are never rewritten, only falsified explicitly
by newer entries. Trust hierarchy: resume section > history > older sections of either.
Log opened 2026-09-18; earlier project history lives in `CHANGELOG.md` and `git log`.

*Resume last rewritten: 2026-09-18 (night). Phase: hybrid session residency — shipped,
proven with a real client (aider); **stacked PR held until serve-bridge PR #197
merges** (still OPEN as of 2026-09-18 night).*
*One-line status: aider confirmed the ladder — non-edit turns prefill 16–18 tokens
(`n_reused` 587 → 1243), edit turns full-clear by design; blockers documented in
ADR-004 with the upstream-PR routing decided; server on LFM2.5 :8017 with `--auto-echo`.*

## State delta (this session)

- **Residency features shipped and proven:** append-only hybrid reuse (`e930b4d`),
  warmup replay (`2cb9cd3`), `--rs-seq` wired but off (`63768e7`), `preserve_reasoning`
  (`d83d153`), bridge `--auto-echo` + aider history canonicalization (`57c654e`,
  `39cc706`). All measured with verified answers; tables in `docs/serve.md`.
- **Confirmed aider telemetry (the design point):** mixed edit/non-edit session on
  LFM2.5 — edits: 426/815 prefilled, 0 reused (full clear, ~18/36 s); follow-ups:
  **18/16 prefilled, 587/1243 reused, ~0.8 s prefill**. Guidance: batch edits, ask
  follow-ups freely.
- **Two aider behaviors found:** boilerplate-on-newest-turn (fixed bridge-side via
  request-independent canonicalization — strip from all user turns, relocate into the
  system prompt once) and edit-turn file re-adds (inherent: new file content must be
  seen; a hybrid cannot rewind over a changed span).
- **Blockers documented and routed (ADR-004):** hybrid edit-turn rewind waits on two
  upstream llama.cpp defects — (1) lfm2moe graph-reserve crash with snapshots (~368-byte
  node-pool shortfall) → **upstream PR against ggml-org/llama.cpp**; (2) non-bit-exact
  snapshot restore (`40` vs `420` under greedy decode) → **not forked now**; any future
  fork experiment goes as a 1-commit branch on `Helldez/llama.cpp` (the submodule
  remote) with maintainer agreement — not a new personal fork.
- **Lifecycle consolidated** to a single `PROGRESS.md` (this file) per user choice.

## Artifacts touched (this session)

| File | What |
|---|---|
| `core/src/engine/session.cpp` | append-reuse + mirror poisoning; `preserve_reasoning` → template kwargs |
| `core/include/bmoe/{config.h,session.h}`, `cli/main.cpp` | `n_rs_seq` / `preserve_reasoning` plumbing (`--rs-seq` flag) |
| `scripts/bmoe-serve.py` | per-request `think`/`preserve_reasoning`; `--auto-echo` (registry + rewrite + canonicalization); `BMOE_DEBUG_ECHO` payload capture |
| `scripts/bench-features.sh` | before/after proof of warmup / echo / `--rs-seq`, verified answers |
| `docs/adr/001..004`, `docs/README.md` | decisions + index |
| `docs/serve.md`, `docs/telemetry.md`, `CHANGELOG.md` | gates, request fields, measured tables |
| this file (consolidated from `SESSION_SUMMARY.md` + `RE_PROGRESS.md` + `PROGRESS.history.md`) | lifecycle |

Branch `feat/session-residency` (stacked on `feat/serve-bridge-arm64`), pushed to
`fork` through `39cc706`. Tags: `progress/2026-09-17-residency-warmup`,
`progress/2026-09-17-snapshot-rollback`, `progress/2026-09-18-reasoning-echo`.

## Environment state

- **Server**: LFM2.5-8B on :8017 with `--auto-echo` (`lfm2moe`, 8k ctx, `--chatml`).
  Daily-driver alternative (Ling-mini): `setsid nohup python3 -u scripts/bmoe-serve.py -m ~/llm/models/Ling-mini-2.0-Q4_K_M.gguf --engine-args "--ctx-size 8192 --chatml" --port 8017 > /tmp/bmoe-serve.log 2>&1 &`
  (add `--auto-echo` for thinking models; Ling-mini does not think).
- **Models** (`~/llm/models/`): Ling-mini-2.0, LFM2.5-8B, Qwen3.5-9B, olmoe-1b-7b,
  Laguna-XS-2.1, Ornith-1.5, Qwen3-30B, Qwen3.6-35B, Cyber-Tiel-35B.
- **Warmup cache** `~/.cache/bmoe-serve/warmup.json`: self-regenerating (the user's
  earlier LFM2.5 chain was lost during bench debugging — bench script now
  backs up/restores it around every run).
- **Remotes**: `origin` = Helldez/BigMoeOnEdge (upstream; PR #197 from fork's
  `feat/serve-bridge-arm64`), `fork` = cjl4hd/BigMoeOnEdge (push target); `gh` authed
  as `cjl4hd`. Submodule: `Helldez/llama.cpp` @ `0e8c83e51` (one sanctioned expert-hook
  commit on upstream).
- **aider scratch repo**: `~/aider-test` (planted `a - b` bug in `calculator.py`).
- Untracked, NOT ours: `.opencode/`, `bmoe-arm64*`, `opencode.json`, `.aider*`, logs.
- Ephemeral: `/tmp/bmoe-serve.log`, `/tmp/bmoe-autoecho.log`, `/tmp/bmoe-reqs.jsonl`
  (only when `BMOE_DEBUG_ECHO=1`), `/tmp/bf-*` bench outputs — all regenerable.

## Open questions / blocked items

1. **Stacked PR held until #197 merges** (user decision). Plan: verify ancestry → sync
   fork main → rebase `feat/session-residency` (serve-bridge commits collapse) →
   `gh pr create --repo Helldez/BigMoeOnEdge --base main --head cjl4hd:feat/session-residency`.
   Fallback if #197 stalls: fork-internal PR (`--repo cjl4hd --base feat/serve-bridge-arm64`), retarget later.
2. **Reserve-crash upstream PR** (ADR-004 §1): drafted against ggml-org/llama.cpp, not
   yet written.
3. Bit-exact snapshot restore: watch #25913 + `[EXPERIMENTAL]` markers; then flip
   `--rs-seq` default (ADR-001 §2, ADR-004 §2).

## Next actions (ordered)

1. **Watch PR #197**; when merged, execute the stacked-PR plan (Open questions 1).
2. **Draft the lfm2moe reserve-fix PR** to ggml-org/llama.cpp (Open questions 2).
3. **Measure Ling-mini edit-turn reuse** with captured aider payloads — the free
   transformer rewind (ADR-004 Consequences).
4. **Opencode re-test** with `--auto-echo` on LFM2.5 — stable tool-schema prefix should
   reuse even better than aider.
5. **Daily driver**: Ling-mini on :8017 when the benchmarking session ends.

## Resume gates (all must assert positives)

1. `cmake --build build -j4 2>&1 | grep -E 'error|warning'` → empty (clean build).
2. `cd build && ctest --output-on-failure` → **13/13 passed**.
3. `git status -sb` → `feat/session-residency` in sync with fork, clean tree;
   `git log --oneline -1` = newest residency-arc commit.
4. `curl -fsS -m 3 http://127.0.0.1:8017/v1/models` → the `bmoe-local` JSON.
5. `bash -n scripts/bench-features.sh && python3 -m py_compile scripts/bmoe-serve.py` → silent.

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
