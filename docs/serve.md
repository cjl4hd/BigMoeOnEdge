# Serving the engine to agent tooling

The engine has no HTTP server. It serves prompt requests over the `--session` stdin protocol
([telemetry.md](telemetry.md#session-mode)), which is what the Android app drives. For desktop
agent tooling — opencode, or anything that speaks the OpenAI Chat Completions API — a small
bridge wraps that protocol in an OpenAI-compatible HTTP endpoint, without adding an HTTP stack
or any dependency to the engine itself.

- `scripts/bmoe-serve.py` — stdlib-only Python bridge and the single entry point: spawns
  `bmoe-cli --session`, translates `POST /v1/chat/completions` (streaming and not) and
  `GET /v1/models` onto the line protocol, and keeps the model loaded between requests so the
  expert cache stays warm. It resolves engine and model itself (`--engine`/`--model`, else
  `$BMOE_ENGINE`/`$BMOE_MODEL`, else the host build of a repo checkout, else a `bmoe-cli`
  sitting beside the script in a staged bundle), with friendly errors instead of a raw
  subprocess failure.

```bash
python3 scripts/bmoe-serve.py -m ~/llm/models/LFM2.5-8B-A1B-UD-Q4_K_M.gguf \
    --model-id lfm2.5-8b-a1b --port 8017 \
    --engine-args "--chatml --moe-stream --ctx-size 16384 --ubatch 512"
```

`BMOE_ENGINE` overrides the engine binary, e.g. a cross-built ARM64 bundle. Point `--host
0.0.0.0` to serve other machines on the LAN; the default is loopback only.

What the bridge does with client fields:

- `messages` are forwarded verbatim to the engine when the conversation is plain text (see
  *Session residency* below); non-text content falls back to the flattened single-prompt path.
  `--chatml` makes the engine render the model's own GGUF-embedded chat template.
- `max_tokens` (default 2048 — thinking models spend completion tokens on reasoning) becomes
  `n_predict`, and is **clamped to the bridge's `--max-tokens` ceiling**: a client asking for
  more output than fits beside its prompt would otherwise sit in the n_ctx window for tens of
  minutes. OpenAI clients treat `max_tokens` as a ceiling, so clamping is behavior-preserving.
  If a request still overflows `n_ctx`, the bridge retries once with the budget halved
  (floor 256) rather than failing — and if the prompt alone overflows, the error surfaces,
  because that genuinely needs a shorter conversation.
- Requests are serialised: the engine is one session, one generation at a time.
- Non-streaming responses carry a `bmoe` object with the `BMOE_DONE` perf block (tok/s, cache
  hit, stall) — the same numbers the CSV sink records. The SSE stream emits only OpenAI-shaped
  chunks: strict client SDKs validate every event, so no vendor-specific events ride the stream.

## Session residency: second turns are cheap

Plain-text conversations are forwarded to the engine **verbatim** (`messages` array, not the
flattened string), so the engine owns the conversation state and the bridge stays stateless. The
engine renders its chat template over the full array each turn and keeps the KV prefix of the
longest common conversation — compaction, edited messages and retries reduce to the same
truncate-and-extend path, and no cache-coherence logic exists anywhere because every request
carries the authoritative history.

The effect on a prefill-bound host is the whole point: the first turn prefills everything, and
every following turn prefills **only the new tokens** — a turn that appends one message re-prefills
a few hundred tokens instead of the whole conversation. `BMOE_DONE` reports this as `n_reused`
(KV prefix carried over) alongside `n_prompt` (tokens actually prefilled this turn). Conversations
with image parts or non-text content fall back to the flattened one-shot prompt path.

**Hybrid/recurrent architectures (lfm2moe, qwen35moe, …) are excluded from prefix reuse:** a
partial `seq_rm` rewinds positions but not the per-sequence cell state carried in the same memory,
so decoding after a mid-sequence rewind fails outright (`llama_decode: failed to decode, ret = 2`,
first seen as dead LFM2.5 sessions the moment a client rewrote its history). Those models
re-prefill every turn — the pre-residency behavior — while keeping the engine-held conversation,
and a cancelled turn likewise forces the next turn's full re-prefill there.

**One exception: pure appends.** When a hybrid turn's rendered prompt is a strict extension of the
resident token mirror — the client echoed the previous assistant reply verbatim and only added
messages — the diff finds the full prompt already resident and no `seq_rm` runs at all. Cells only
ever grow, so the reuse is as safe as a first prefill, and a continuation turn skips its whole
prefill. Two properties bound this in practice: the mirror includes the *generated* tokens, so the
client must echo the previous reply verbatim (aider and the bridge do; hand-built histories do
not); and any failed, cancelled or overflowed turn resets the mirror so the next turn full-clears —
cell state may sit past what the mirror describes. Thinking hybrids (LFM2.5) cannot hit this path
at all: the reasoning tokens live in the cache between the prompt and the answer, but a client
echoes only the answer, so the render always diverges before the append point. Append reuse on
hybrids is therefore a property of non-thinking hybrid stacks (e.g. qwen35moe with
`enable_thinking=false`).

## Warmup: the first query is cheap too

Residency helps from the second turn on; the **warmup** moves the first turn's cost to server
start. After each request the bridge saves the stable prefix — everything but the in-flight user
turn — to `~/.cache/bmoe-serve/warmup.json` (mode 0600; it holds conversation contents). At
startup the bridge replays that prefix through the messages path as short prefill-only segments
(`n_predict=0`), so the model's own template renders what the next session will send and the
engine holds it resident before any client connects. A client's first query then diffs against
the warm prefix and prefills only its own delta.

The design is speculative-safe by construction: if the next session sends a different system
prompt, tool schema or repo map, the residency diff truncates at the first divergence and only
the mismatched tail is prefilled — a mismatch wastes compute, never correctness. The saved prefix
is keyed to the model file, so switching models skips the replay, and `--no-warmup` disables it.
Segments release the engine lock between segments, so a request arriving mid-warmup preempts the
rest without losing what is already resident.

For observability the bridge prints one `TELEMETRY` line per request to its log — `n_prompt`,
`n_reused`, `tokens`, `prefill_s`, `tok_s`, `wall_s`, … — because agent clients (aider, opencode)
discard the perf block.

## Host results — the residency/warmup effect, measured

All rows from a 2015-era 8-thread laptop (12 GB RAM, SATA-class storage), `--ctx-size 16384
--ubatch 512 --moe-stream`, served through the bridge. These measure the *serving* mechanics
(warmup, residency, expert I/O), not peak throughput — see [benchmarks.md](benchmarks.md) for the
device matrix and its fixed protocol.

| Model (arch) | Load | Cold 1st turn | Warm turn | Decode | Notes |
|---|---|---|---|---|---|
| Ling-mini-2.0 Q4 (`bailingmoe2`, 16.5B/1.4B) | 27 s | 58 s | **12 s** (202 reused / 34 prefilled) | 3–4 tok/s | no thinking phase; best quality/speed balance |
| OLMoE-1B-7B Q4 (`olmoe`, 6.9B/1.0B) | 12 s | 17 s | **3 s** (219 reused / 21 prefilled) | 9–14 tok/s | wordy, older; lowest RAM (≈4.7 GB headroom) |
| Laguna XS 2.1 Q4 (`laguna`, 33B/3B) | 61–96 s | 127 s | 15–24 s | ~1.4 tok/s | strongest quality; cold-expert I/O dominates |

Reading the numbers:

- **Warm turns are the design point.** The delta between cold and warm is the expert I/O plus the
  prefix prefill that residency removed; what remains is decode plus a fixed per-request cost as
  the expert cache churns. On a model whose whole file fits in page cache (OLMoE, 4.2 GB), warm
  prefill reached 22 tok/s and the warm turn is decode-bound pure and simple.
- **Warmup frontloads the first turn.** Same conversation replayed after a restart: 219 tokens
  prefilled cold → **12 prefilled / 207 reused** with warmup replayed at startup.
- **Prefill speed is page-cache-bound, not compute-bound, on this host.** The same model measured
  3.7 tok/s prefill cold and 27 tok/s warm within one session; the engine's O_DIRECT path wins
  when RAM cannot hold the file, and the page cache wins when it can.
- **Client interaction quality (measured, same host):** aider's `whole` edit format applies
  single-turn edits reliably on Ling-mini but multi-turn rewrites silently drop earlier changes;
  the `diff` format exceeded the model's ability (aider retried until timeout). opencode's
  tool-call editing completed, but at ~1B-class quality the model hallucinated tool output —
  treat these models as Q&A/drafting engines, not autonomous agents.

## Memory budget on the host

Compute buffers are reserved for the widest graph, and the dominant term scales with
`ubatch × vocabulary` — at ctx 8192 on LFM2.5-8B that measured **4.1 GiB** on a desktop host,
enough to OOM-kill the session the first time a generation ran with ~4 GiB free. `--ubatch 512`
caps the reservation at ~258 MiB and leaves decode speed untouched (only prefill splits into
more graphs). On RAM-constrained hosts, keep `--ubatch 512` and let `--cache-mb auto` size the
expert cache to what is left.

## Prefill is the wall on small hardware

Prompt processing (prefill) is compute-bound, and on modest CPUs it dwarfs everything else:
a 2015 dual-core laptop measured **~20 tok/s**, so an agent client that re-sends its multi-
thousand-token system prompt pays minutes before the first token. The engine-side knobs do
not move it — decode threads (2 vs 4), `--ubatch` 256/512/1024, and the CPU governor all
land within run-to-run noise; `--n-expert-used` does, but it drops the model's real routing
and answer quality with it (a benchmark knob, not a service one). The lever that works is
shrinking what the client sends, which is a client configuration problem:

- Set the model's real limits so agent clients compact before overflowing
  (`context` deliberately below the engine's `--ctx-size` to leave output headroom):

```jsonc
"lfm2.5-8b-a1b": {
  "name": "LFM2.5-8B-A1B (bmoe streamed)",
  "limit": { "context": 12288, "output": 2048 }
}
```

- Prefer a minimal agent profile: a ~100-token custom prompt instead of the client's full
  agent stack, and fewer enabled tools — every enabled tool's schema rides in the prompt on
  every request. In opencode that is a `"mode": "primary"` agent with a short `prompt`, the
  mutating tools disabled, and the bmoe model pinned.
- Keep conversations short: clients re-send the whole history each turn, and `/new` is
  cheaper than any cache.

## ARM64 Linux bundles

`scripts/build-arm64.sh` cross-compiles the engine for ARM64 **GNU/Linux** (needs the
`aarch64-linux-gnu` GCC toolchain) and stages a self-contained `bmoe-arm64/` bundle — the CLI
plus its shared libraries, RUNPATH `$ORIGIN/lib`, no `LD_LIBRARY_PATH` required:

```bash
scripts/build-arm64.sh            # stage bmoe-arm64/ (and bmoe-arm64.tar.gz with --tar)
scp -r bmoe-arm64/ target:/opt/   # then on the target:
/opt/bmoe-arm64/bmoe-cli -m model.gguf --moe-stream -p "hello"
```

Baseline is `armv8.2-a+dotprod+fp16` (any 2018+ ARM64 SoC; no i8mm, so older SoCs do not
SIGILL). The expert-ready hook is compiled in, so `--overlap` works. This is **not** the
Android build — Android binaries link bionic and only run on Android. See the bundle's own
`bmoe-arm64/README.md` for running the server from it (`BMOE_ENGINE=bmoe-arm64/bmoe-cli`).

Configuring the client (opencode) is a user-level concern, not a repo one: add an
`@ai-sdk/openai-compatible` provider pointing at `http://127.0.0.1:8017/v1`, either per project
or in `~/.config/opencode/opencode.json` for every session, and give the model the `limit`
from the prefill section above.
