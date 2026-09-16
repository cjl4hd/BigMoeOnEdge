#!/usr/bin/env bash
# Serve bmoe-cli as an OpenAI-compatible HTTP endpoint for local agent tooling
# (opencode, or anything that speaks /v1/chat/completions).
#
# Usage: scripts/run-server.sh [-m model.gguf] [extra bmoe-serve.py args]
#   The model comes from -m, or $BMOE_MODEL, or falls back to the tiny test model —
#   which exists after a host build and proves the plumbing, but is useless for real work.
#
# Example (30B MoE, streamed experts, 8k context):
#   scripts/run-server.sh -m ~/llm/models/Qwen3-30B-A3B-Q4_K_M.gguf \
#       --engine-args "--moe-stream --ctx-size 8192" --port 8017
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${BMOE_ENGINE:-$ROOT/build/cli/bmoe-cli}"   # e.g. BMOE_ENGINE=bmoe-arm64/bmoe-cli
[ -x "$ENGINE" ] || { echo "engine not built: $ENGINE — run scripts/build-host.sh (or set BMOE_ENGINE)" >&2; exit 1; }

MODEL=""
ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        -m) MODEL="$2"; shift 2 ;;
        *) ARGS+=("$1"); shift ;;
    esac
done
MODEL="${MODEL:-${BMOE_MODEL:-$ROOT/build/tests/tiny-moe-qwen3moe.gguf}}"
[ -f "$MODEL" ] || { echo "model not found: $MODEL" >&2; exit 1; }

exec python3 "$ROOT/scripts/bmoe-serve.py" --engine "$ENGINE" --model "$MODEL" "${ARGS[@]}"
