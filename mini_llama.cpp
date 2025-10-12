// mini_llama.cpp (stable, template-free; no llama_batch_get_one)
// Usage:
//   ./mini_llama "Write a haiku about autumn." [models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf]

#include "llama.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Robust tokenizer (resizes if initial buffer too small)
static bool tokenize_text(const llama_vocab * vocab,
                          const std::string & text,
                          std::vector<llama_token> & out,
                          bool add_special = true,
                          bool parse_special = false) {
    int32_t cap = (int32_t)text.size() + 8;
    if (cap < 32) cap = 32;
    out.resize(cap);

    int32_t n = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(),
                               out.data(), cap, add_special, parse_special);
    if (n < 0) {
        out.resize(-n);
        n = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(),
                           out.data(), (int32_t)out.size(),
                           add_special, parse_special);
    }
    if (n <= 0) return false;
    out.resize(n);
    return true;
}

int main(int argc, char **argv) {
    const char * user_text  = (argc > 1) ? argv[1] : "Say hi in one sentence.";
    const char * model_path = (argc > 2) ? argv[2] : "models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";

    // 1) backend
    llama_backend_init();

    // 2) model
    llama_model_params mp = llama_model_default_params();
    llama_model * model = llama_model_load_from_file(model_path, mp);
    if (!model) {
        std::fprintf(stderr, "Failed to load model: %s\n", model_path);
        return 1;
    }

    // 3) context
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx   = 1024;
    cp.n_batch = 1024;
    llama_context * ctx = llama_init_from_model(model, cp);
    if (!ctx) {
        std::fprintf(stderr, "Failed to create context\n");
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    if (!vocab) {
        std::fprintf(stderr, "Failed to get vocab\n");
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    // 4) simple, template-free prompt
    std::string prompt;
    prompt += "You are a concise, helpful assistant.\n";
    prompt += "User: ";
    prompt += user_text;
    prompt += "\nAssistant:";

    // 5) tokenize (safe defaults for SPM/BPE)
    std::vector<llama_token> toks;
    if (!tokenize_text(vocab, prompt, toks,
                       /*add_special=*/true,
                       /*parse_special=*/false)) {
        std::fprintf(stderr, "tokenize failed\n");
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    // 6) feed prompt
    llama_batch batch = llama_batch_init((int)toks.size(), /*embd*/0, /*n_seq_max*/1);
    for (int i = 0; i < (int)toks.size(); ++i) {
        batch.token[i]     = toks[i];
        batch.pos[i]       = i;
        batch.seq_id[i][0] = 0;
        batch.n_seq_id[i]  = 1;
        batch.logits[i]    = (i == (int)toks.size() - 1);
    }
    batch.n_tokens = (int)toks.size();

    if (llama_decode(ctx, batch) != 0) {
        std::fprintf(stderr, "decode failed (prompt)\n");
        llama_batch_free(batch);
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    // 7) sampler chain
    llama_sampler_chain_params scp = llama_sampler_chain_default_params();
    llama_sampler * smpl = llama_sampler_chain_init(scp);
    if (!smpl) {
        std::fprintf(stderr, "sampler init failed\n");
        llama_batch_free(batch);
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.9f, /*min_keep=*/1));
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    const llama_token tok_eos = llama_vocab_eos(vocab);

    // 8) generation: reuse a 1-token batch (no get_one)
    llama_batch next = llama_batch_init(/*n_tokens=*/1, /*embd=*/0, /*n_seq_max=*/1);

    const int max_new_tokens = 128;
    int n_past = (int)toks.size();

    std::printf("\n[Model output]: ");
    for (int t = 0; t < max_new_tokens; ++t) {
        const llama_token id = llama_sampler_sample(smpl, ctx, /*idx_last*/-1);
        llama_sampler_accept(smpl, id);
        if (id == tok_eos) break;

        // print piece (use a roomy buffer)
        char piece[1024];
        const int wrote = llama_token_to_piece(vocab, id, piece, (int)sizeof(piece),
                                               /*lstrip=*/0, /*special=*/true);
        if (wrote > 0) {
            std::fwrite(piece, 1, wrote, stdout);
            std::fflush(stdout);
        }

        // fill "next" batch explicitly
        next.n_tokens      = 1;
        next.token[0]      = id;
        next.pos[0]        = n_past;
        next.seq_id[0][0]  = 0;
        next.n_seq_id[0]   = 1;
        next.logits[0]     = true;

        if (llama_decode(ctx, next) != 0) {
            std::fprintf(stderr, "\ndecode failed (generation)\n");
            break;
        }
        ++n_past;
    }
    std::printf("\n");

    // 9) cleanup
    llama_batch_free(next);
    llama_sampler_free(smpl);
    llama_batch_free(batch);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
