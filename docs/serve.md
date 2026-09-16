# Serving the engine to agent tooling

The engine has no HTTP server. It serves prompt requests over the `--session` stdin protocol
([telemetry.md](telemetry.md#session-mode)), which is what the Android app drives. For desktop
agent tooling — opencode, or anything that speaks the OpenAI Chat Completions API — a small
bridge wraps that protocol in an OpenAI-compatible HTTP endpoint, without adding an HTTP stack
or any dependency to the engine itself.

- `scripts/bmoe-serve.py` — stdlib-only Python bridge: spawns `bmoe-cli --session`, translates
  `POST /v1/chat/completions` (streaming and not) and `GET /v1/models` onto the line protocol,
  and keeps the model loaded between requests so the expert cache stays warm.
- `scripts/run-server.sh` — launcher that resolves the engine and a model file:

```bash
scripts/run-server.sh -m ~/llm/models/LFM2.5-8B-A1B-UD-Q4_K_M.gguf \
    --model-id lfm2.5-8b-a1b --port 8017 \
    --engine-args "--chatml --moe-stream --ctx-size 4096 --ubatch 512"
```

`BMOE_ENGINE` overrides the engine binary, e.g. a cross-built ARM64 bundle. Point `--host
0.0.0.0` to serve other machines on the LAN; the default is loopback only.

What the bridge does with client fields:

- `messages` are flattened to a single user prompt (system messages kept inline); the engine
  renders its own chat template over its own conversation history (`--chatml` required).
- `max_tokens` (default 2048 — thinking models spend completion tokens on reasoning) becomes
  `n_predict`; reasoning arrives separately in `reasoning_content`, streamed or not.
- Requests are serialised: the engine is one session, one generation at a time.
- Every response carries a `bmoe` object with the `BMOE_DONE` perf block (tok/s, cache hit,
  stall) — the same numbers the CSV sink records.

## Memory budget on the host

Compute buffers are reserved for the widest graph, and the dominant term scales with
`ubatch × vocabulary` — at ctx 8192 on LFM2.5-8B that measured **4.1 GiB** on a desktop host,
enough to OOM-kill the session the first time a generation ran with ~4 GiB free. `--ubatch 512`
caps the reservation at ~258 MiB and leaves decode speed untouched (only prefill splits into
more graphs). On RAM-constrained hosts, keep `--ubatch 512` and let `--cache-mb auto` size the
expert cache to what is left.

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
or in `~/.config/opencode/opencode.json` for every session.
