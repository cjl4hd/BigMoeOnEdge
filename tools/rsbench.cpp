// bmoe-rsbench — reproduce and measure the two hybrid-recurrent-state blockers (ADR-004)
// against the llama.cpp this engine links, using the engine's exact context shape
// (core/src/engine/session.cpp: n_ubatch = n_batch, use_extra_bufts=false).
//
//   bmoe-rsbench reserve <model.gguf>      : create a context with n_rs_seq=64 and decode.
//                                            Reproduces the LFM2/LFM2MOE graph-reserve crash
//                                            (GGML_ASSERT(obj_new), needed 836640 vs available
//                                            836272 — one ggml_tensor object short). Fixed
//                                            upstream by adding the LFM2 archs to the
//                                            linear-attention node budget (ggml-org PR #29085).
//   bmoe-rsbench diverge  <model.gguf> [N] : one rollback cell (single-mode rm, d=3, m=0)
//                                            compared against an identically-fed reference
//                                            context. Exit 0 = EXACT, 5 = DIFFER.
//   bmoe-rsbench sweep    <model.gguf>     : run the depth x post-batch-steps x batch-shape
//                                            matrix on one model. Each cell runs TWO contexts
//                                            with identical token feeds (A: rollback+replay,
//                                            B: plain continuation) and byte-compares their
//                                            greedy streams. Cells print predicted vs observed;
//                                            exit 0 iff every cell matches its prediction.
//
// Snapshot mechanics (pin @ 0e8c83e51, src/llama-memory-recurrent.cpp, src/models/delta-net-base.cpp,
// ggml/src/ggml-cpu/ops.cpp) that the predictions encode:
//   - a ubatch of n_seq_tokens tokens writes only min(n_seq_tokens, K) snapshot planes
//     (K = n_rs_seq + 1); single-token steps therefore refresh ONLY plane 0, leaving
//     planes 1..K-1 at their values from the last multi-token ubatch (GDN staleness);
//   - the conv path's in-ubatch sweep maps slot t to the state (n_seq_tokens - (ch-1) ... )
//     clamped at 0, so a 1-token step collapses ALL K slots to the same "1 back" state
//     (conv too-new);
//   - seq_rm rollback depth d reads plane d ("d back") via the per-seq rs_idx gather, so it
//     is exact only while the needed planes still hold their true "d back" values.
// Measured law (2026-09-18, both qwen35 and lfm2moe, engine-shaped context — see
// docs/adr/004 addendum 2 for the full matrix):
//   m = 0  (rollback directly after the last multi-token ubatch):
//     EXACT at every swept depth on qwen35 (single and batched rm alike), and on lfm2moe at
//     d = 1, 3, 24 — i.e. upstream's snapshot restore roundtrip is exact in the regime its
//     fixture test exercises, and the earlier "non-bit-exact restore" claims were an artifact
//     of a harness whose replay wiped its own pending rollback with a full seq_rm(-1,-1).
//   m >= 1 (single-token decode steps between the last multi-token ubatch and the rollback):
//     DIFFER on BOTH families at every swept depth and rm shape. Single-token steps refresh
//     only snapshot plane 0 (a ubatch writes min(n_seq_tokens, K) planes), so planes d >= 1
//     go stale — this is the server's real edit-turn shape and the reason --rs-seq stays
//     default-off. A candidate upstream fix: replay the m trailing tokens through one
//     ubatch (refreshing the planes) before restoring, or maintain the planes per token.
//   Residual anomaly (unresolved): lfm2moe at m=0, d=8 differs identically in both rm shapes,
//     which neither family-wide mechanism above explains (see ADR-004, open item).
// Hence per cell (mode, d_rm, m) the tool predicts DIFFER iff m >= 1 (plus the lfm2moe d=8
// m=0 exception when running that arch), and prints predicted-vs-observed per cell.
//
// Public API only (llama.h). Build via the engine's normal configure:
//   cmake -B build -DBMOE_BUILD_TOOLS=ON && cmake --build build --target bmoe-rsbench
#include "llama.h"
#include <algorithm>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kNRsSeq   = 32;  // >= max d_rm swept; plane count is K = kNRsSeq + 1
constexpr uint32_t kNCtx     = 256; // prompt + m + d_rm + n_gen must fit
constexpr int      kMaxDRm   = 24;
constexpr int      kMaxM     = 16;

struct Cell {
    const char * mode;   // "single" | "batched"
    int d_rm;            // rollback depth (tokens cut)
    int m;               // single-token decode steps between last multi-token ubatch and seq_rm
};

llama_model * load_model(const char * path, const llama_vocab * & vocab) {
    llama_model_params mp = llama_model_default_params();
    mp.use_extra_bufts = false;  // engine load shape (session.cpp): no repack/extra buffers
    llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { fprintf(stderr, "bmoe-rsbench: model load failed\n"); exit(1); }
    vocab = llama_model_get_vocab(model);
    return model;
}

llama_context * make_ctx(llama_model * model, uint32_t n_ctx, uint32_t n_rs_seq) {
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx     = n_ctx;
    cp.n_batch   = n_ctx;
    cp.n_ubatch  = n_ctx;  // engine default: n_ubatch = n_batch
    cp.n_seq_max = 1;
    cp.n_rs_seq  = n_rs_seq;
    cp.n_threads = cp.n_threads_batch = 4;
    return llama_init_from_model(model, cp);
}

std::vector<llama_token> tokenize(const llama_vocab * vocab, const char * text) {
    int n = -llama_tokenize(vocab, text, (int)strlen(text), nullptr, 0, true, true);
    std::vector<llama_token> toks(n);
    llama_tokenize(vocab, text, (int)strlen(text), toks.data(), (int)toks.size(), true, true);
    return toks;
}

llama_token sample_greedy(llama_context * ctx) {
    const float * logits = llama_get_logits_ith(ctx, -1);
    const llama_vocab * vocab = llama_model_get_vocab(llama_get_model(ctx));
    int n_vocab = llama_vocab_n_tokens(vocab);
    int best = 0;
    for (int i = 1; i < n_vocab; i++) {
        if (logits[i] > logits[best]) {
            best = i;
        }
    }
    return (llama_token) best;
}

// Decode one token batch (positions auto-assigned from the cell's pos).
bool decode(llama_context * ctx, const llama_token * toks, int n) {
    llama_batch batch = llama_batch_get_one(const_cast<llama_token *>(toks), n);
    return llama_decode(ctx, batch) == 0;
}

std::string detok(const llama_vocab * vocab, const std::vector<llama_token> & toks) {
    std::string s(1024, '\0');
    int n = llama_detokenize(vocab, toks.data(), (int)toks.size(), s.data(), (int)s.size(), 0, true);
    if (n < 0) { s.resize((size_t)(-n) + 1); n = llama_detokenize(vocab, toks.data(), (int)toks.size(), s.data(), (int)s.size(), 0, true); }
    s.resize(n > 0 ? (size_t)n : 0);
    return s;
}

// Greedy continuation from the context's current state. Does NOT touch memory —
// the caller owns all seq_rm/clear decisions (the old diverge mode wiped its pending
// rollback here; that bug invalidated the first exactness baselines).
std::vector<llama_token> gen_greedy(llama_context * ctx, int n_gen, const llama_vocab * vocab) {
    std::vector<llama_token> out;
    llama_token tok = sample_greedy(ctx);
    out.push_back(tok);
    while ((int)out.size() < n_gen && tok != llama_vocab_eos(vocab)) {
        if (!decode(ctx, &tok, 1)) { fprintf(stderr, "bmoe-rsbench: gen decode failed\n"); exit(2); }
        tok = sample_greedy(ctx);
        out.push_back(tok);
    }
    return out;
}

// First index where the streams differ, or -1 if equal (streams may end early on EOS).
int first_diff(const std::vector<llama_token> & a, const std::vector<llama_token> & b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (int) i;
        }
    }
    return a.size() == b.size() ? -1 : (int) n;
}

const char * predict(const Cell & c) {
    const bool single = strcmp(c.mode, "single") == 0;
    if (single) {
        if (c.m == 0) {
            return c.d_rm == 1 ? "EXACT" : "DIFFER";  // d>=2: conv collapse
        }
        return "DIFFER";  // GDN plane stale by m; conv too-new
    }
    // batched
    return c.m == 0 ? "EXACT" : "DIFFER";  // m>=1: boundary plane d_rm stale by m
}

// One cell = two contexts with identical token feeds; A rolls back and replays,
// B just continues. Byte-compare the n_gen greedy tokens after the feed point.
// Returns: 0 = ran, observed EXACT; 1 = ran, observed DIFFER; 2 = infrastructure
// failure (seq_rm refused / decode failed / reserve sanity broke).
int run_cell(llama_model * model, const llama_vocab * vocab, const std::vector<llama_token> & prompt,
             const std::vector<llama_token> & seed, const Cell & c, bool verbose,
             std::vector<llama_token> & stream_a, std::vector<llama_token> & stream_b) {
    const int P    = (int) prompt.size();
    const int n_rm = c.mode[0] == 's' ? c.d_rm : 1;  // batched rm phase = one d_rm-token decode

    llama_context * ctx_a = make_ctx(model, kNCtx, kNRsSeq);
    llama_context * ctx_b = make_ctx(model, kNCtx, kNRsSeq);
    if (!ctx_a || !ctx_b) {
        printf("CELL %s d=%d m=%d : RESERVE_FAIL (context creation returned null)\n", c.mode, c.d_rm, c.m);
        if (ctx_a) { llama_free(ctx_a); }
        if (ctx_b) { llama_free(ctx_b); }
        return 2;
    }

    // Sanity: both contexts must have snapshots engaged (n_rs_seq survives the allowlist clamp).
    if (llama_n_rs_seq(ctx_a) != kNRsSeq) {
        printf("CELL %s d=%d m=%d : SKIP (n_rs_seq clamped to %u — arch not on rollback allowlist)\n",
               c.mode, c.d_rm, c.m, llama_n_rs_seq(ctx_a));
        llama_free(ctx_a);
        llama_free(ctx_b);
        return 2;
    }

    int status = 2;
    do {
        // Phase 1+2 (both): prefill prompt, then m single-token steps on the seed chain.
        llama_memory_t mem_a = llama_get_memory(ctx_a);
        for (int pass = 0; pass < 2; ++pass) {
            llama_context * ctx = pass == 0 ? ctx_a : ctx_b;
            if (!decode(ctx, prompt.data(), P)) { fprintf(stderr, "bmoe-rsbench: prefill failed\n"); goto done; }
            for (int i = 0; i < c.m; ++i) {
                if (!decode(ctx, &seed[i], 1)) { fprintf(stderr, "bmoe-rsbench: seed step failed\n"); goto done; }
            }
        }

        // Phase 3 (both): the d_rm tokens that A will roll back. single: one decode per token
        // (refreshes only plane 0); batched: one d_rm-token ubatch (refreshes planes 0..d_rm-1).
        for (int pass = 0; pass < 2; ++pass) {
            llama_context * ctx = pass == 0 ? ctx_a : ctx_b;
            for (int i = 0; i < n_rm; ++i) {
                const int n_tok = c.mode[0] == 's' ? 1 : c.d_rm;
                if (!decode(ctx, seed.data() + c.m + i * n_tok, n_tok)) {
                    fprintf(stderr, "bmoe-rsbench: rm-phase decode failed\n");
                    goto done;
                }
                if (c.mode[0] != 's') {
                    break;  // batched: the whole rm phase is one decode
                }
            }
        }

        // Phase 4 (A only): the rollback — single-use, must succeed.
        if (!llama_memory_seq_rm(mem_a, 0, P + c.m, -1)) {
            printf("CELL %s d=%d m=%d : RM_FAIL (seq_rm refused — snapshots not engaged)\n", c.mode, c.d_rm, c.m);
            goto done;
        }

        // Phase 5 (A): replay the cut tail as one batch, then greedy n_gen.
        {
            llama_batch batch = llama_batch_get_one(const_cast<llama_token *>(seed.data() + c.m), c.d_rm);
            if (llama_decode(ctx_a, batch)) { fprintf(stderr, "bmoe-rsbench: replay decode failed\n"); goto done; }
        }
        stream_a = gen_greedy(ctx_a, 8, vocab);

        // Phase 5' (B): continue from the same feed point with no rollback.
        stream_b = gen_greedy(ctx_b, (int) stream_a.size(), vocab);

        status = first_diff(stream_a, stream_b) < 0 ? 0 : 1;
    } while (false);

done:
    llama_free(ctx_a);
    llama_free(ctx_b);
    return status;
}

// Sensitivity probe: does the greedy stream notice a state difference at all on this
// prompt? S1 = prefill, 8 single steps, FULL-CLEAR memory, re-prefill the same 8 tokens
// (zeroed recurrent state), gen. S2 = identical feed without the clear. If the two streams
// match, the prompt's argmax is state-insensitive and EXACT matrix cells are uninformative.
void sensitivity_probe(llama_model * model, const llama_vocab * vocab,
                       const std::vector<llama_token> & prompt, const std::vector<llama_token> & seed) {
    llama_context * s1 = make_ctx(model, kNCtx, 0);
    llama_context * s2 = make_ctx(model, kNCtx, 0);
    if (!s1 || !s2) { printf("SENSITIVITY: context creation failed\n"); return; }
    for (int pass = 0; pass < 2; ++pass) {
        llama_context * ctx = pass == 0 ? s1 : s2;
        decode(ctx, prompt.data(), (int)prompt.size());
        for (int i = 0; i < 8; ++i) { decode(ctx, &seed[i], 1); }
    }
    llama_memory_seq_rm(llama_get_memory(s1), 0, -1, -1);  // full clear (s1 only)
    decode(s1, seed.data(), 8);                            // re-prefill on zeroed state
    const std::vector<llama_token> g1 = gen_greedy(s1, 8, vocab);
    const std::vector<llama_token> g2 = gen_greedy(s2, (int)g1.size(), vocab);
    const bool same = first_diff(g1, g2) < 0;
    printf("SENSITIVITY: %s\n", same ?
           "INSENSITIVE — full-clear vs continuation streams match; EXACT cells are uninformative on this prompt"
         : "SENSITIVE — state differences change the stream; EXACT cells are meaningful");
    fflush(stdout);
    llama_free(s1);
    llama_free(s2);
}

void run_sweep(llama_model * model, const llama_vocab * vocab, bool fox_mode) {
    // Number-chain prompt: short prefill, and the greedy tail keeps counting (never EOS),
    // so the golden seed chain always covers m + d_rm + n_gen tokens. fox mode uses the
    // original (state-sensitive) prompt at ~4x the prefill cost, 4 decisive cells only.
    const char * prompt_text = fox_mode ?
        "The quick brown fox jumps over the lazy dog. Repeat after me, exactly and only: hello world\n" :
        "Count from 1 to 100: 1 2 3 4 5 6 7 8 9";
    const std::vector<llama_token> prompt = tokenize(vocab, prompt_text);
    const int P = (int) prompt.size();

    // Golden chain: feeds every cell (m seed tokens + d_rm rolled-back tokens + gen).
    // Computed once on a snapshot-free context so cell comparisons never depend on it.
    llama_context * gold = make_ctx(model, kNCtx, 0);
    if (!gold) { printf("SWEEP: context creation failed\n"); return; }
    if (!decode(gold, prompt.data(), P)) { printf("SWEEP: prefill failed\n"); llama_free(gold); return; }
    std::vector<llama_token> seed = gen_greedy(gold, kMaxM + kMaxDRm + 8, vocab);
    llama_free(gold);
    if ((int) seed.size() < kMaxM + kMaxDRm + 4) {
        printf("SWEEP: golden chain too short (%zu tokens, EOS early) — extend the prompt\n", seed.size());
        return;
    }

    sensitivity_probe(model, vocab, prompt, seed);

    static const Cell fox_cells[] = {
        {"batched", 3, 0}, {"batched", 8, 4}, {"single", 3, 0}, {"single", 8, 4}
    };
    static const Cell std_cells[] = {
        {"batched",  1, 0}, {"batched",  3, 0}, {"batched",  8, 0}, {"batched", 24, 0},
        {"batched",  1, 4}, {"batched",  3, 4}, {"batched",  8, 4}, {"batched", 24, 4},
        {"single",   1, 0}, {"single",   3, 0}, {"single",   8, 0}, {"single",  24, 0},
        {"single",   1, 4}, {"single",   3, 4}, {"single",   8, 4}, {"single",  24, 4}
    };
    const Cell * cells     = fox_mode ? fox_cells : std_cells;
    const int    n_cells   = fox_mode ? (int)(sizeof(fox_cells) / sizeof(fox_cells[0]))
                                     : (int)(sizeof(std_cells) / sizeof(std_cells[0]));

    char arch_buf[64] = {0};
    llama_model_meta_val_str(model, "general.architecture", arch_buf, sizeof(arch_buf));
    printf("SWEEP %s | prompt=%d tok, n_rs_seq=%u, n_gen=8, axes: mode x d_rm x m\n",
           arch_buf[0] ? arch_buf : "unknown-arch", P, kNRsSeq);
    fflush(stdout);

    int n_pred_ok = 0, n_pred_bad = 0, n_infra = 0;
    for (int ci = 0; ci < n_cells; ++ci) {
        const Cell & c = cells[ci];
        std::vector<llama_token> sa, sb;
        const int rc = run_cell(model, vocab, prompt, seed, c, false, sa, sb);
        const char * pred = predict(c);
        if (rc == 2) {
            n_infra++;
            printf("CELL %d/%d %s d=%d m=%d : INFRA-FAIL\n", ci + 1, n_cells, c.mode, c.d_rm, c.m);
            fflush(stdout);
            continue;
        }
        const bool exact = rc == 0;
        const bool ok    = (strcmp(pred, "EXACT") == 0) == exact;
        ok ? n_pred_ok++ : n_pred_bad++;

        std::string ta, tb;
        const int fd = first_diff(sa, sb);
        if (!exact && fd >= 0) {
            ta = detok(vocab, {sa[fd]});
            tb = detok(vocab, {sb[fd]});
        }
        printf("CELL %d/%d %s d=%2d m=%2d : %s (pred %s)%s%s%s\n", ci + 1, n_cells, c.mode, c.d_rm, c.m,
               exact ? "EXACT" : "DIFFER", pred, ok ? "" : "  << PREDICTION FALSIFIED",
               exact ? "" : "  first diff tok: A=\"", exact ? "" : (ta + "\" B=\"" + tb + "\"").c_str());
        fflush(stdout);
    }
    printf("SWEEP SUMMARY: %d/%d cells matched predictions (%d infra failures)\n",
           n_pred_ok, n_pred_ok + n_pred_bad, n_infra);
    if (n_pred_bad == 0 && n_infra == 0) {
        printf("HYPOTHESIS: SUPPORTED — snapshot-plane staleness + conv 1-slot collapse explain every cell\n");
    } else if (n_pred_bad > 0) {
        printf("HYPOTHESIS: FALSIFIED in %d cell(s) — refine the mechanism\n", n_pred_bad);
    }
}

} // namespace

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bmoe-rsbench reserve|diverge <model.gguf> [n_gen] | sweep <model.gguf>\n");
        return 1;
    }
    const char * mode       = argv[1];
    const char * model_path = argv[2];
    int n_gen = argc > 3 ? atoi(argv[3]) : 24;

    llama_model * model = nullptr;
    const llama_vocab * vocab = nullptr;
    model = load_model(model_path, vocab);
    setvbuf(stdout, nullptr, _IOLBF, 0);

    if (strcmp(mode, "reserve") == 0) {
        llama_context * ctx = make_ctx(model, 2048, 64);
        if (!ctx) { printf("RESERVE: FAIL — context creation returned null\n"); return 3; }
        std::vector<llama_token> toks = tokenize(vocab, "hello");
        llama_batch batch = llama_batch_get_one(toks.data(), (int)toks.size());
        if (llama_decode(ctx, batch)) {
            printf("RESERVE: CRASH or decode failure at first decode (see stderr above)\n");
            return 4;
        }
        printf("RESERVE: OK — n_rs_seq=64 context created and decoded\n");
        llama_free(ctx);
        llama_model_free(model);
        return 0;
    }

    if (strcmp(mode, "diverge") == 0) {
        // Fixed cell: single-mode rm phase, d_rm=3 (the fixture-test depth), m=0.
        // Reference = an identically-fed context that never rolls back (matched shape —
        // the old version compared against a fresh-context decode with different memory history).
        const char * prompt_text =
            "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. "
            "Repeat after me, exactly and only: hello world\n";
        const std::vector<llama_token> prompt = tokenize(vocab, prompt_text);
        std::vector<llama_token> seed(4 + 3 + 128);
        Cell c = {"single", 3, 0};
        std::vector<llama_token> sa, sb;
        // seed chain: reuse the golden from a no-snapshot context
        llama_context * gold = make_ctx(model, kNCtx, 0);
        if (!gold) { printf("DIVERGE: context creation failed\n"); return 3; }
        decode(gold, prompt.data(), (int)prompt.size());
        seed = gen_greedy(gold, (int)seed.size(), vocab);
        llama_free(gold);
        const int rc = run_cell(model, vocab, prompt, seed, c, true, sa, sb);
        if (rc == 2) { printf("DIVERGE: infrastructure failure\n"); return 4; }
        printf("DIVERGE: %s\n", rc == 0 ? "EXACT" : "DIFFER");
        printf("  restored : \"%s\"\n", detok(vocab, sa).c_str());
        printf("  reference: \"%s\"\n", detok(vocab, sb).c_str());
        llama_model_free(model);
        return rc == 0 ? 0 : 5;
    }

    if (strcmp(mode, "sweep") == 0) {
        run_sweep(model, vocab, argc > 3 && strcmp(argv[3], "fox") == 0);
        llama_model_free(model);
        return 0;
    }

    fprintf(stderr, "bmoe-rsbench: unknown mode\n");
    return 1;
}
