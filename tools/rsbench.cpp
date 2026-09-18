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
//   bmoe-rsbench diverge  <model.gguf> [N] : greedy-decode a fixed prompt twice — once fresh,
//                                            once after a recurrent-state snapshot rollback —
//                                            and report whether the token streams match.
//                                            Exit 0 = EXACT, 5 = DIFFER. On the gated-delta-net
//                                            hybrids (qwen35, lfm2 families) upstream restore is
//                                            NOT bit-exact: this is the standing proof why
//                                            --rs-seq stays default-off (ADR-001/ADR-004).
//
// Public API only (llama.h). Build via the engine's normal configure:
//   cmake -B build -DBMOE_BUILD_TOOLS=ON && cmake --build build --target bmoe-rsbench
#include "llama.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static llama_model * load_model(const char * path, const llama_vocab * & vocab) {
    llama_model_params mp = llama_model_default_params();
    mp.use_extra_bufts = false;  // engine load shape (session.cpp): no repack/extra buffers
    llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { fprintf(stderr, "bmoe-rsbench: model load failed\n"); exit(1); }
    vocab = llama_model_get_vocab(model);
    return model;
}

static llama_context * make_ctx(llama_model * model, uint32_t n_ctx, uint32_t n_rs_seq) {
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx     = n_ctx;
    cp.n_batch   = n_ctx;
    cp.n_ubatch  = n_ctx;  // engine default: n_ubatch = n_batch
    cp.n_seq_max = 1;
    cp.n_rs_seq  = n_rs_seq;
    cp.n_threads = cp.n_threads_batch = 4;
    return llama_init_from_model(model, cp);
}

static std::vector<llama_token> tokenize(const llama_vocab * vocab, const char * text) {
    int n = -llama_tokenize(vocab, text, (int)strlen(text), nullptr, 0, true, true);
    std::vector<llama_token> toks(n);
    llama_tokenize(vocab, text, (int)strlen(text), toks.data(), (int)toks.size(), true, true);
    return toks;
}

static llama_token sample_greedy(llama_context * ctx) {
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

static std::vector<llama_token> greedy_generate(llama_context * ctx, const llama_vocab * vocab,
                                                std::vector<llama_token> & prompt, int n_gen) {
    llama_memory_t mem = llama_get_memory(ctx);
    llama_memory_seq_rm(mem, 0, -1, -1);
    llama_batch batch = llama_batch_get_one(prompt.data(), (int)prompt.size());
    if (llama_decode(ctx, batch)) { fprintf(stderr, "bmoe-rsbench: prefill decode failed\n"); exit(2); }

    std::vector<llama_token> out;
    llama_token tok = sample_greedy(ctx);
    out.push_back(tok);
    while ((int)out.size() < n_gen && tok != llama_vocab_eos(vocab)) {
        llama_batch one = llama_batch_get_one(&tok, 1);
        if (llama_decode(ctx, one)) { fprintf(stderr, "bmoe-rsbench: gen decode failed\n"); exit(2); }
        tok = sample_greedy(ctx);
        out.push_back(tok);
    }
    return out;
}

static void print_text(const llama_vocab * vocab, const std::vector<llama_token> & toks, const char * label) {
    std::string s(1024, '\0');
    int n = llama_detokenize(vocab, toks.data(), (int)toks.size(), s.data(), (int)s.size(), 0, true);
    if (n < 0) { s.resize((size_t)(-n) + 1); n = llama_detokenize(vocab, toks.data(), (int)toks.size(), s.data(), (int)s.size(), 0, true); }
    s.resize(n > 0 ? (size_t)n : 0);
    printf("  %s: [", label);
    for (size_t i = 0; i < toks.size(); i++) printf("%d%s", toks[i], i + 1 < toks.size() ? " " : "");
    printf("]  \"%s\"\n", s.c_str());
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bmoe-rsbench reserve|diverge <model.gguf> [n_gen]\n");
        return 1;
    }
    const char * mode       = argv[1];
    const char * model_path = argv[2];
    int n_gen = argc > 3 ? atoi(argv[3]) : 24;

    llama_model * model = nullptr;
    const llama_vocab * vocab = nullptr;
    model = load_model(model_path, vocab);

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
        const char * prompt_text =
            "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. "
            "Repeat after me, exactly and only: hello world\n";
        std::vector<llama_token> prompt = tokenize(vocab, prompt_text);

        // Reference: untouched context, decodes the whole prompt, never rolls back.
        llama_context * ref = make_ctx(model, 2048, 0);
        std::vector<llama_token> ref_out = greedy_generate(ref, vocab, prompt, n_gen);

        // Rollback path: decode everything (snapshots captured along the way), then remove the
        // last 3 tokens of the prompt — the same depth upstream's own fixture rollback test uses.
        // With snapshots engaged, seq_rm should succeed and restore the state at the cut.
        llama_context * ctx = make_ctx(model, 2048, 64);
        std::vector<llama_token> full = greedy_generate(ctx, vocab, prompt, 4); // keep pre-gen short
        const llama_pos cut = (llama_pos)(prompt.size() - 3);
        llama_memory_t mem = llama_get_memory(ctx);
        if (!llama_memory_seq_rm(mem, 0, cut, -1)) {
            printf("DIVERGE: seq_rm(rollback) returned false — snapshot mechanism not engaged\n");
            return 4;
        }
        std::vector<llama_token> tail(prompt.begin() + cut, prompt.end());
        std::vector<llama_token> restored = greedy_generate(ctx, vocab, tail, n_gen);
        if ((int)restored.size() > n_gen) {
            restored.erase(restored.begin(), restored.end() - n_gen);
        }

        bool exact = (restored == ref_out);
        printf("DIVERGE: %s\n", exact ? "EXACT" : "DIFFER");
        print_text(vocab, restored, "restored");
        print_text(vocab, ref_out,  "reference");
        llama_free(ctx);
        llama_free(ref);
        llama_model_free(model);
        return exact ? 0 : 5;
    }

    fprintf(stderr, "bmoe-rsbench: unknown mode\n");
    return 1;
}
