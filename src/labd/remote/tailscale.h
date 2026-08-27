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

#ifndef _LABD_REMOTE_TAILSCALE_H_
    #define _LABD_REMOTE_TAILSCALE_H_ (1)

#include "labd/events/bus.h"

namespace chromelab {
    // Manages Tailscale via the `tailscale` CLI
    class TailscaleManager {
    public:
        explicit TailscaleManager(EventBus* bus = nullptr);
        ~TailscaleManager(void) = default;

        TailscaleManager(const TailscaleManager&)            = delete;
        TailscaleManager& operator=(const TailscaleManager&) = delete;

        TSStatus GetStatus(void) const;

        TSStatus Up(const std::string& authkey = "");
        TSStatus Down(void);

        TSStatus GetIP(void) const;

        bool IsUp(void) const;

    private:
        static std::pair<int, std::string> Run(const std::string& cmd);
        void Emit(const std::string& category, const std::string& message, int severity) const;

        EventBus* m_bus;
    };
} // namespace chromelab

#endif // _LABD_REMOTE_TAILSCALE_H_
