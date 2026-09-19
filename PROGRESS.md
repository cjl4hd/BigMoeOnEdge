# PROGRESS — session entry point + evidence log

**Structure (per the project-lifecycle skill):** the resume section below is rewritten
every session — never append to it. Under the divider, the append-only history log holds
the long-form evidence narrative; entries are never rewritten, only falsified explicitly
by newer entries. Trust hierarchy: resume section > history > older sections of either.
Log opened 2026-09-18; earlier project history lives in `CHANGELOG.md` and `git log`.

*Resume last rewritten: 2026-09-18 (late night, session 3). Phase: hybrid residency — the
snapshot-rollback mechanism is fully mapped and FIXED on `cjl4hd/llama.cpp`
branch `fix/rs-rollback-index-shift` (index-shift restore + honest refusals + delta-net
conv alignment); verification is argmax-level with per-cell shape controls because the
backend is ubatch-shape dependent (~3 logits of noise) — which also resolved the d=8
anomaly.*
*One-line status: vanilla restores plane d of the last multi-token ubatch, exact only for
m=0 rollbacks cutting into that ubatch (d < its token count); the fix reads plane d−m when
the wanted state survives and refuses honestly otherwise (cut cells 9/9 EXACT on lfm2moe
and qwen35 vs vanilla's 3/9). PR #29085 (reserve, separate) and stacked PR #197 still
OPEN; the fix branch is committed locally, its PR not yet opened.*

## State delta (this session)

- **The mechanism, end to end (kernel-traced, then measured):** a ubatch of n tokens
  writes snapshot planes 0..min(n,K)−1, plane p = state p tokens before the ubatch's end
  (GDN kernel `ops.cpp`: `target_slot = n_tokens−1−t`; `lfm2.cpp` conv: `n_written =
  min(n,K)`); single-token steps rewrite only plane 0; `seq_rm` reads plane d. Vanilla is
  therefore exact iff the wanted state still occupies plane d — m=0 rollbacks cutting into
  the last multi-token ubatch only. Every other shape restores a stale or never-written
  state. The "lfm2moe m=0 d=8 anomaly" is resolved: the sweep's rm ubatch had exactly d
  tokens, so plane d was never written by it — an arch-independent read of garbage, not an
  LFM quirk.
- **The backend is ubatch-shape dependent (~O(1) logits).** With NO rollback anywhere,
  splitting a 10-token prefill 6+4 moves logits by up to 3.6 (MoE routing flips amplify
  accumulation-order noise). This invalidates every bitwise rollback-vs-reference
  comparison whose two sides saw different ubatch shapes — including upstream's own
  multi-seq fixture, which FAILS on vanilla master (max diff 11.6) because it compares a
  12-token-ubatch history against a 10-token-ubatch history at eps=1e-7. The fixture now
  probes shape noise in-test and downgrades its bitwise assertions to reported-not-
  asserted on shape-dependent backends. All earlier `statecmp`/`dsteps` "restore is not
  bitwise" results carry the same confound; only the d=0 identical-shape control was
  meaningful (it passed).
- **The fix (branch `fix/rs-rollback-index-shift` in `~/git/llama.cpp`, committed):**
  per-seq epoch bookkeeping (`rs_epoch_end` / `rs_epoch_planes` / `rs_epoch_lo`) set per
  multi-token ubatch in `find_slot`; `seq_rm` restores plane `d−m` when the wanted state
  survives the timeline and refuses otherwise (destroyed planes, checkpoint-loaded state —
  a state blob carries a single plane — cleared/invalidated seqs, pending rollback);
  `delta-net-base.cpp` now writes min(n,K) conv slots like lfm2.cpp (was: clamped all K,
  desyncing conv from GDN planes after single-token steps); `prepare` dry-run, `rm_all`,
  tail invalidation, fresh starts, `seq_cp` (inherit) / `seq_add` (affine follow) /
  `seq_div` (invalidate) all handled. Code-reviewer findings fixed: discarded-timeline
  planes after rollback+single-replay (the `rs_epoch_lo` floor), stale epochs surviving
  sequence teardown, OOB in the tool's diff printing.
- **Verification (new `cutsweep`: c tokens cut into the prefill × m singles, per-cell
  shape-control row):** fix build EXACT 9/9 cells on lfm2moe AND qwen35; vanilla 3/9 EXACT
  + 4 DIFFER + 2 REFUSED (c+m > n_rs_seq). Fixture test passes honestly on both cache
  fills (single-seq bitwise incl. the checkpoint round-trips; multi-seq shape-gated),
  after being reshaped to the sound decode-then-rollback shape and to assert the new
  refusal semantics.

## Artifacts touched (this session)

| File | What |
|---|---|
| `tools/rsbench.cpp` | new `cutsweep` mode (cut-into-prefill cells + per-cell shape-control rows, `kCutRsSeq=8`); top-of-file law comment updated to the resolved mechanism; OOB guard on diff printing; INFRA diagnostics |
| `docs/adr/004` | Addendum 3 (mechanism, shape-noise confound, the fix, cutsweep evidence); Addendum 2 marked superseded-in-part |
| `CHANGELOG.md` | 0.24.3: cutsweep + d=8 anomaly resolution + upstream fix branch |
| this file | resume rewrite + history entry (session 3) |
| `~/git/llama.cpp` branch `fix/rs-rollback-index-shift` | `src/llama-memory-recurrent.{h,cpp}` (epoch bookkeeping + index-shift `seq_rm`), `src/models/delta-net-base.cpp` (conv min(n,K)), `tests/test-recurrent-state-rollback.cpp` (reshape + shape-noise gate) — committed locally |
| `/tmp/pr-index-shift-description-draft.md` | PR description starting point (template; user rewrites as human) |
| `/tmp/bmoe-rsbench-clone` | clone-linked runner (regenerate: `g++ -O2 -std=c++17 -I ~/git/llama.cpp/include -I ~/git/llama.cpp/ggml/include tools/rsbench.cpp -o /tmp/bmoe-rsbench-clone ~/git/llama.cpp/build/bin/libllama.so ~/git/llama.cpp/build/bin/libggml.so ~/git/llama.cpp/build/bin/libggml-base.so -Wl,-rpath,$HOME/git/llama.cpp/build/bin`) |

Evidence (ephemeral, regenerable): `/tmp/cutsweep-fix-lfm.txt`, `/tmp/cutsweep-fix-q35.txt`
(fix build), `/tmp/cutsweep-vanilla-lfm.txt`, `/tmp/cutsweep-vanilla-q35.txt` (vanilla
baseline; regenerate via `git stash push -- src/ tests/` in the clone, rebuild `llama`,
run, `git stash pop`, rebuild). Vanilla-master fixture failure log: rerun
`test-recurrent-state-rollback -m <lfm2 gguf>` on a stash-cleaned build. Earlier
sweep/statecmp outputs from session 2 remain regenerable via the same commands.

Branch `feat/session-residency` (stacked on `feat/serve-bridge-arm64`), pushed to
`fork` through `02f9278`. Tags: `progress/2026-09-17-residency-warmup`,
`progress/2026-09-17-snapshot-rollback`, `progress/2026-09-18-reasoning-echo`.
Upstream: `fix/rs-rollback-index-shift` pushed to `cjl4hd/llama.cpp` and opened as
**ggml-org/llama.cpp#29117 (DRAFT)** — description is still the tool draft; the user
rewrites it as a human before leaving draft. #29085 remains in draft with its existing
human-authored body untouched.

## Environment state

- **Server**: LFM2.5-8B on :8017 with `--auto-echo` (`lfm2moe`, 8k ctx, `--chatml`).
  Daily-driver alternative (Ling-mini): `setsid nohup python3 -u scripts/bmoe-serve.py -m ~/llm/models/Ling-mini-2.0-Q4_K_M.gguf --engine-args "--ctx-size 8192 --chatml" --port 8017 > /tmp/bmoe-serve.log 2>&1 &`
  (add `--auto-echo` for thinking models; Ling-mini does not think).
- **Models** (`~/llm/models/`): Ling-mini-2.0, LFM2.5-8B-A1B-UD-Q4_K_M (note: no plain
  `-Q4_K_M` file — sweeps use the UD file), Qwen3.5-9B, olmoe-1b-7b, Laguna-XS-2.1,
  Ornith-1.5, Qwen3-30B, Qwen3.6-35B, Cyber-Tiel-35B.
- **Warmup cache** `~/.cache/bmoe-serve/warmup.json`: self-regenerating.
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
3. **Open the fix PR (next).** Branch `fix/rs-rollback-index-shift` in `~/git/llama.cpp`
   is complete and verified locally; push it to `cjl4hd/llama.cpp` and open the PR against
   ggml-org from it. Separate from #29085 (reserve). The description must be written as a
   human per the ggml-org bot's rules — `/tmp/pr-index-shift-description-draft.md` is only
   a starting point following the PR template. Flag explicitly: `seq_rm` now returns false
   where it used to return true (destroyed states); callers ignoring the return value will
   hit the position-check decode failure and fall back to re-prefill.
4. **Shape-dependent-backend caveat for any future bitwise claim:** any exactness
   comparison against a differently-shaped reference is meaningless here (~3 logits of
   noise from ubatch splits alone, MoE routing flips). Only identical-shape controls
   (d=0) or argmax-level verdicts with shape-control rows are admissible evidence.

## Next actions (ordered)

1. **Humanize PR #29117's description** (it is the tool draft verbatim) and reply to
   reviewer/bot feedback: `gh pr view 29117 --repo ggml-org/llama.cpp --web`. Keep it a
   draft until the description is your own words. Same for #29085 (in draft, body already
   reads final — review once, then ready-for-review).
2. **Engine-side enablement decision** (after the PR is up): once an upstream release
   carries the fix, `--rs-seq` + hybrid edit turns become viable — plan the `--rs-seq`
   flip condition and the hybrid residency un-exclusion (CHANGELOG 0.24.2's exclusion
   note) for a future session; requires a submodule bump + full gates per ADR-001.
3. **Watch #29085 and #197** (`gh pr view 29085 --repo ggml-org/llama.cpp`); execute the
   stacked-PR plan when #197 merges; do the bump + gates when #29085 merges.
4. **Measure Ling-mini edit-turn reuse** with captured aider payloads — the free
   transformer rewind (ADR-004 Consequences).
5. **Opencode re-test** with `--auto-echo` on LFM2.5 — stable tool-schema prefix should
   reuse even better than aider.
6. **Daily driver**: Ling-mini on :8017 when the benchmarking session ends.

## Resume gates (all must assert positives)

1. `cmake --build build -j4 2>&1 | grep -E 'error|warning'` → empty (clean build).
2. `cd build && ctest --output-on-failure` → **13/13 passed**.
3. `git status -sb` → `feat/session-residency` in sync with fork, clean tree;
   `git log --oneline -1` = newest residency-arc commit.
4. `curl -fsS -m 3 http://127.0.0.1:8017/v1/models` → the `bmoe-local` JSON.
5. `bash -n scripts/bench-features.sh && python3 -m py_compile scripts/bmoe-serve.py` → silent.
6. `test -x build/tools/bmoe-rsbench` → exists (needs `-DBMOE_BUILD_TOOLS=ON`).
7. `./build/tools/bmoe-rsbench reserve <lfm2 gguf>` → exit 134 on the unfixed pin
   (regression signal for the reserve repro; flips to 0 after the #29085 bump).
8. `cd ~/git/llama.cpp && git branch --show-current && git status --short` →
   `fix/rs-rollback-index-shift`, clean tree, in sync with origin (the fix is committed
   and pushed); `gh pr view 29117 --repo ggml-org/llama.cpp --json isDraft` → `true`;
   re-verify cutsweep with `/tmp/bmoe-rsbench-clone cutsweep <lfm2 gguf>` → 9/9 EXACT
   (rebuild the runner and the clone `llama` target first if the branch moved).

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
