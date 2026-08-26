#pragma region LICENSE

// MIT License
//
// Copyright (c) 2026 Rohan Bharatia
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma endregion LICENSE

#include "labd/ai/engine.h"
#include "labd/events/bus.h"

namespace chromelab {
    AiEngine::AiEngine(const std::string& models_dir, int64_t max_ram, EventBus* bus)
        : m_models_dir(models_dir),
        m_max_ram(max_ram),
        m_bus(bus) {
        llama_backend_init();
    }

    AiEngine::~AiEngine(void) {
        ReleaseModel();
        llama_backend_free();
    }

    std::vector<ModelInfo> AiEngine::ListModels(void) const {
        std::vector<ModelInfo> result;

        std::error_code ec;
        if (!std::filesystem::is_directory(m_models_dir, ec)) {
            return result;
        }

        for (const auto& entry : std::filesystem::directory_iterator(m_models_dir, ec)) {
            if (!entry.is_regular_file()) {
                continue;
            } if (entry.path().extension() != ".gguf") {
                continue;
            }

            ModelInfo info;
            info.set_name(entry.path().stem().string());
            info.set_filename(entry.path().filename().string());

            auto sz = std::filesystem::file_size(entry, ec);
            if (!ec) {
                info.set_size_bytes(static_cast<int64_t>(sz));
            }

            // Extract quantization hint from filename (e.g. "model-Q4_K_M.gguf")
            auto name = entry.path().stem().string();
            auto dash = name.rfind('-');
            if (dash != std::string::npos) {
                info.set_quantization(name.substr(dash + 1));
            }

            std::lock_guard lock(m_mutex);
            info.set_loaded(m_model != nullptr && m_loaded_model_name == info.filename());

            result.push_back(std::move(info));
        }

        return result;
    }

    AIStatus AiEngine::GetStatus(void) const {
        std::lock_guard lock(m_mutex);
        AIStatus status;
        if (m_model != nullptr) {
            status.set_status(MODEL_STATUS_READY);
            status.set_current_model(m_loaded_model_name);
        } else {
            status.set_status(MODEL_STATUS_UNLOADED);
        }
        return status;
    }

    AIStatus AiEngine::Load(const std::string& model_name) {
        std::lock_guard lock(m_mutex);

        if (m_model != nullptr) {
            ReleaseModel();
        }

        AIStatus status;
        status.set_status(MODEL_STATUS_LOADING);

        // Find the file
        std::string path = m_models_dir + "/" + model_name;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            status.set_status(MODEL_STATUS_ERROR);
            status.set_error("model not found: " + model_name);
            return status;
        }

        // Load model
        auto model_params = llama_model_default_params();
        m_model           = llama_model_load_from_file(path.c_str(), model_params);
        if (!m_model) {
            status.set_status(MODEL_STATUS_ERROR);
            status.set_error("failed to load model: " + model_name);
            return status;
        }

        m_vocab = llama_model_get_vocab(m_model);

        // Create context
        auto ctx_params    = llama_context_default_params();
        ctx_params.n_ctx   = 4096;
        ctx_params.n_batch = 512;
        m_ctx              = llama_init_from_model(m_model, ctx_params);
        if (!m_ctx) {
            llama_model_free(m_model);
            m_model  = nullptr;
            m_vocab  = nullptr;
            status.set_status(MODEL_STATUS_ERROR);
            status.set_error("failed to create context for: " + model_name);
            return status;
        }

        // Create sampler chain (greedy for now, temperature applied per-request)
        auto sparams = llama_sampler_chain_default_params();
        m_sampler    = llama_sampler_chain_init(sparams);

        m_loaded_model_name = model_name;
        status.set_status(MODEL_STATUS_READY);
        status.set_current_model(model_name);

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_AI);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("ai-engine");
            ev.set_message("Model loaded: " + model_name);
            m_bus->Emit(ev);
        }

        return status;
    }

    AIStatus AiEngine::Unload(void) {
        std::lock_guard lock(m_mutex);

        AIStatus status;
        if (m_model == nullptr) {
            status.set_status(MODEL_STATUS_UNLOADED);
            return status;
        }

        std::string name = m_loaded_model_name;
        ReleaseModel();
        status.set_status(MODEL_STATUS_UNLOADED);

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_AI);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("ai-engine");
            ev.set_message("Model unloaded: " + name);
            m_bus->Emit(ev);
        }

        return status;
    }

    AiEngine::CompletionResult AiEngine::ChatComplete(const std::vector<ChatMessage>& messages, int32_t max_tokens, double temperature) {
        std::lock_guard lock(m_mutex);
        CompletionResult result;

        if (!m_model || !m_ctx) {
            return result;
        }

        // Build prompt from messages
        std::string prompt;
        for (const auto& msg : messages) {
            if (msg.role() == "system") {
                prompt += "<|system|>\n" + msg.content() + "\n";
            } else if (msg.role() == "user") {
                prompt += "<|user|>\n" + msg.content() + "\n";
            } else if (msg.role() == "assistant") {
                prompt += "<|assistant|>\n" + msg.content() + "\n";
            }
        }
        prompt += "<|assistant|>\n";

        // Tokenize prompt
        int32_t n_prompt = llama_tokenize(m_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()), nullptr, 0, true, true);
        if (n_prompt < 0) {
            result.content = "tokenization failed";
            return result;
        }

        std::vector<llama_token> tokens(n_prompt);
        llama_tokenize(m_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()), tokens.data(), n_prompt, true, true);

        // Clear KV cache
        llama_memory_clear(llama_get_memory(m_ctx), false);

        // Evaluate prompt
        auto t0           = std::chrono::steady_clock::now();
        llama_batch batch = llama_batch_get_one(tokens.data(), n_prompt);
        if (llama_decode(m_ctx, batch) != 0) {
            result.content = "decode failed";
            return result;
        }

        auto t1               = std::chrono::steady_clock::now();
        result.prompt_eval_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Generate
        auto eos = llama_vocab_eos(m_vocab);
        t0       = std::chrono::steady_clock::now();

        for (int32_t i = 0; i < max_tokens; ++i) {
            const float* logits = llama_get_logits(m_ctx);

            // Build a sampler chain per-token with temperature
            auto sp                            = llama_sampler_chain_default_params();
            struct llama_sampler* temp_sampler = llama_sampler_chain_init(sp);
            llama_sampler_chain_add(temp_sampler, llama_sampler_init_temp(static_cast<float>(temperature)));
            llama_sampler_chain_add(temp_sampler, llama_sampler_init_dist(0));

            llama_token new_token = llama_sampler_sample(temp_sampler, m_ctx, -1);
            llama_sampler_free(temp_sampler);

            llama_sampler_accept(m_sampler, new_token);

            if (llama_vocab_is_eog(m_vocab, new_token)) break;

            char buf[256];
            int32_t n = llama_token_to_piece(m_vocab, new_token, buf, sizeof(buf) - 1, 0, true);
            if (n > 0) {
                buf[n] = '\0';
                result.content += buf;
            }
            ++result.tokens_generated;

            // Decode the new token
            llama_batch new_batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(m_ctx, new_batch) != 0) {
                break;
            }
        }

        t1 = std::chrono::steady_clock::now();
        result.eval_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        return result;
    }

    void AiEngine::StreamChat(const std::vector<ChatMessage>& messages, int32_t max_tokens, double temperature, TokenCallback cb) {
        std::lock_guard lock(m_mutex);

        if (!m_model || !m_ctx) {
            cb("[error: no model loaded]", true);
            return;
        }

        // Build prompt
        std::string prompt;
        for (const auto& msg : messages) {
            if (msg.role() == "system") {
                prompt += "<|system|>\n" + msg.content() + "\n";
            } else if (msg.role() == "user") {
                prompt += "<|user|>\n" + msg.content() + "\n";
            } else if (msg.role() == "assistant") {
                prompt += "<|assistant|>\n" + msg.content() + "\n";
            }
        }
        prompt += "<|assistant|>\n";

        // Tokenize
        int32_t n_prompt = llama_tokenize(m_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()), nullptr, 0, true, true);
        if (n_prompt < 0) {
            cb("[error: tokenization failed]", true);
            return;
        }

        std::vector<llama_token> tokens(n_prompt);
        llama_tokenize(m_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()), tokens.data(), n_prompt, true, true);

        llama_memory_clear(llama_get_memory(m_ctx), false);

        llama_batch batch = llama_batch_get_one(tokens.data(), n_prompt);
        if (llama_decode(m_ctx, batch) != 0) {
            cb("[error: decode failed]", true);
            return;
        }

        auto eos = llama_vocab_eos(m_vocab);

        for (int32_t i = 0; i < max_tokens; ++i) {
            auto sp                            = llama_sampler_chain_default_params();
            struct llama_sampler* temp_sampler = llama_sampler_chain_init(sp);
            llama_sampler_chain_add(temp_sampler, llama_sampler_init_temp(static_cast<float>(temperature)));
            llama_sampler_chain_add(temp_sampler, llama_sampler_init_dist(0));

            llama_token new_token = llama_sampler_sample(temp_sampler, m_ctx, -1);
            llama_sampler_free(temp_sampler);

            llama_sampler_accept(m_sampler, new_token);

            if (llama_vocab_is_eog(m_vocab, new_token)) {
                cb("", true);
                break;
            }

            char buf[256];
            int32_t n = llama_token_to_piece(m_vocab, new_token, buf, sizeof(buf) - 1, 0, true);
            if (n > 0) {
                buf[n] = '\0';
                if (!cb(buf, false)) break;
            }

            llama_batch new_batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(m_ctx, new_batch) != 0) {
                break;
            }
        }
    }

    void AiEngine::ReleaseModel(void) {
        if (m_sampler) {
            llama_sampler_free(m_sampler);
            m_sampler = nullptr;
        } if (m_ctx) {
            llama_free(m_ctx);
            m_ctx = nullptr;
        }
        if (m_model) {
            llama_model_free(m_model);
            m_model  = nullptr;
        }
        m_vocab = nullptr;
        m_loaded_model_name.clear();
    }
} // namespace chromelab
