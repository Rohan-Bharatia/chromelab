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

#pragma once

#ifndef _LABD_AI_ENGINE_H_
    #define _LABD_AI_ENGINE_H_ (1)

#include "labd/events/bus.h"

namespace chromelab {
    // Manages a single GGUF model via llama.cpp
    class AiEngine {
    public:
        using TokenCallback = std::function<bool(const std::string& token, bool done)>;

        explicit AiEngine(const std::string& models_dir, int64_t max_ram, EventBus* bus = nullptr);
        ~AiEngine(void);

        AiEngine(const AiEngine&)            = delete;
        AiEngine& operator=(const AiEngine&) = delete;

        std::vector<ModelInfo> ListModels(void) const;

        AIStatus GetStatus(void) const;

        AIStatus Load(const std::string& model_name);

        AIStatus Unload(void);

        struct CompletionResult {
            std::string content;
            int32_t tokens_generated = 0;
            double prompt_eval_ms    = 0;
            double eval_ms           = 0;
        };

        CompletionResult ChatComplete(const std::vector<ChatMessage>& messages, int32_t max_tokens, double temperature);
        void StreamChat(const std::vector<ChatMessage>& messages,int32_t max_tokens, double temperature, TokenCallback cb);

    private:
        void ReleaseModel(void);

        std::string m_models_dir;
        int64_t m_max_ram;
        EventBus* m_bus;

        mutable std::mutex m_mutex;
        std::string m_loaded_model_name;
        struct llama_model* m_model       = nullptr;
        struct llama_context* m_ctx       = nullptr;
        const struct llama_vocab* m_vocab = nullptr;
        struct llama_sampler* m_sampler   = nullptr;
    };
} // namespace chromelab

#endif // _LABD_AI_ENGINE_H_
