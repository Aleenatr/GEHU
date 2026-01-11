#include "llama.h"
#include <android/log.h>
#include <jni.h>
#include <string>
#include <vector>
#include <cstring>

#define LOG_TAG "LlamaNative"

extern "C" {

// ---------------------------
// loadModel(path) : Long
// ---------------------------
JNIEXPORT jlong JNICALL
Java_com_example_demolition_ai_LlamaNative_loadModel(
        JNIEnv *env, jobject /*thiz*/, jstring jModelPath) {

    const char *path = env->GetStringUTFChars(jModelPath, nullptr);
    if (!path) {
        return 0L;
    }

    llama_model_params mparams = llama_model_default_params();
    llama_model *model = llama_model_load_from_file(path, mparams);

    env->ReleaseStringUTFChars(jModelPath, path);
    return reinterpret_cast<jlong>(model);
}

// ---------------------------
// createContext(modelPtr) : Long
// ---------------------------
JNIEXPORT jlong JNICALL
Java_com_example_demolition_ai_LlamaNative_createContext(
        JNIEnv *env, jobject /*thiz*/, jlong modelPtr) {

    if (modelPtr == 0L) {
        return 0L;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 8192;   // long context support

    llama_context *ctx =
            llama_init_from_model(reinterpret_cast<llama_model *>(modelPtr), cparams);

    return reinterpret_cast<jlong>(ctx);
}

// ---------------------------
// generateText(ctxPtr, prompt)
// ---------------------------
JNIEXPORT jstring JNICALL
Java_com_example_demolition_ai_LlamaNative_generateText(
        JNIEnv *env, jobject /*thiz*/, jlong ctxPtr, jstring jPrompt) {

    if (ctxPtr == 0L) {
        return env->NewStringUTF("Error: Invalid context pointer");
    }

    const char *prompt = env->GetStringUTFChars(jPrompt, nullptr);
    if (!prompt) {
        return env->NewStringUTF("Error: Failed to read prompt");
    }

    llama_context *ctx = reinterpret_cast<llama_context *>(ctxPtr);

    // Defensive: clear KV cache (important if contexts are reused later)
    llama_kv_cache_clear(ctx);

    const llama_model *model = llama_get_model(ctx);
    if (!model) {
        env->ReleaseStringUTFChars(jPrompt, prompt);
        return env->NewStringUTF("Error: Model not found");
    }

    const llama_vocab *vocab = llama_model_get_vocab(model);
    if (!vocab) {
        env->ReleaseStringUTFChars(jPrompt, prompt);
        return env->NewStringUTF("Error: Vocabulary not found");
    }

    __android_log_print(
            ANDROID_LOG_DEBUG,
            LOG_TAG,
            "Prompt length (bytes): %zu",
            strlen(prompt)
    );

    // ---------------------------
    // Tokenization (Gemma-safe)
    // ---------------------------
    std::vector<llama_token> tokens;

    size_t estimated_tokens = (strlen(prompt) / 3) + 32;
    if (estimated_tokens > 2048) {
        env->ReleaseStringUTFChars(jPrompt, prompt);
        return env->NewStringUTF("Error: Prompt too long");
    }

    tokens.resize(estimated_tokens);

    int n_tokens = llama_tokenize(
            vocab,
            prompt,
            strlen(prompt),
            tokens.data(),
            tokens.size(),
            true,
            false
    );

    if (n_tokens < 0) {
        size_t required = static_cast<size_t>(-n_tokens);
        if (required > 2048) {
            env->ReleaseStringUTFChars(jPrompt, prompt);
            return env->NewStringUTF("Error: Prompt too long");
        }

        tokens.resize(required);
        n_tokens = llama_tokenize(
                vocab,
                prompt,
                strlen(prompt),
                tokens.data(),
                tokens.size(),
                true,
                false
        );

        if (n_tokens < 0) {
            env->ReleaseStringUTFChars(jPrompt, prompt);
            return env->NewStringUTF("Error: Tokenization failed");
        }
    }

    if (n_tokens == 0) {
        env->ReleaseStringUTFChars(jPrompt, prompt);
        return env->NewStringUTF("Error: Empty prompt");
    }

    tokens.resize(n_tokens);

    // ---------------------------
    // Decode prompt batch
    // ---------------------------
    llama_batch prompt_batch = llama_batch_init(n_tokens, 0, 1);

    for (int i = 0; i < n_tokens; i++) {
        prompt_batch.token[i] = tokens[i];
        prompt_batch.pos[i] = i;
        prompt_batch.n_seq_id[i] = 1;
        prompt_batch.seq_id[i][0] = 0;
        prompt_batch.logits[i] = false;
    }
    prompt_batch.logits[n_tokens - 1] = true;
    prompt_batch.n_tokens = n_tokens;

    if (llama_decode(ctx, prompt_batch) != 0) {
        llama_batch_free(prompt_batch);
        env->ReleaseStringUTFChars(jPrompt, prompt);
        return env->NewStringUTF("Error: Failed to decode prompt");
    }

    llama_batch_free(prompt_batch);

    // ---------------------------
    // Sampler (no greedy override)
    // ---------------------------
    llama_sampler_chain_params sampler_params =
            llama_sampler_chain_default_params();

    llama_sampler *sampler = llama_sampler_chain_init(sampler_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.3f));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));

    // ---------------------------
    // Generation loop
    // ---------------------------
    std::string output;
    const int max_tokens = 512;

    llama_batch gen_batch = llama_batch_init(1, 0, 1);

    for (int i = 0; i < max_tokens; i++) {
        llama_token token = llama_sampler_sample(sampler, ctx, -1);

        if (llama_vocab_is_eog(vocab, token)) {
            break;
        }

        char buf[256];
        int n = llama_token_to_piece(
                vocab,
                token,
                buf,
                sizeof(buf),
                0,
                false
        );

        if (n > 0) {
            output.append(buf, n);
        }

        gen_batch.token[0] = token;
        gen_batch.pos[0] = n_tokens + i;
        gen_batch.n_seq_id[0] = 1;
        gen_batch.seq_id[0][0] = 0;
        gen_batch.logits[0] = true;
        gen_batch.n_tokens = 1;

        if (llama_decode(ctx, gen_batch) != 0) {
            break;
        }
    }

    llama_batch_free(gen_batch);
    llama_sampler_free(sampler);

    env->ReleaseStringUTFChars(jPrompt, prompt);

    if (output.empty()) {
        return env->NewStringUTF("Error: No output generated");
    }

    return env->NewStringUTF(output.c_str());
}

// ---------------------------
// freeContext
// ---------------------------
JNIEXPORT void JNICALL
Java_com_example_demolition_ai_LlamaNative_freeContext(
        JNIEnv *env, jobject /*thiz*/, jlong ctxPtr) {

    if (ctxPtr != 0L) {
        llama_free(reinterpret_cast<llama_context *>(ctxPtr));
    }
}

// ---------------------------
// freeModel
// ---------------------------
JNIEXPORT void JNICALL
Java_com_example_demolition_ai_LlamaNative_freeModel(
        JNIEnv *env, jobject /*thiz*/, jlong modelPtr) {

    if (modelPtr != 0L) {
        llama_model_free(reinterpret_cast<llama_model *>(modelPtr));
    }
}

} // extern "C"
