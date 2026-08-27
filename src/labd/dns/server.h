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

#ifndef _LABD_DNS_SERVER_H_
    #define _LABD_DNS_SERVER_H_ (1)

#include "labd/events/bus.h"

namespace chromelab {
    // Lightweight DNS forwarder/cacher
    class DnsServer {
    public:
        DnsServer(const std::string& domain, EventBus* bus = nullptr);
        ~DnsServer(void);

        DnsServer(const DnsServer&)            = delete;
        DnsServer& operator=(const DnsServer&) = delete;

        bool Start(uint16_t port = 53);
        void Stop(void);
        bool IsRunning(void) const;

        // Query stats.
        int64_t QueriesTotal(void) const;
        int64_t CacheHits(void) const;

    private:
        void RecvLoop(void);
        std::vector<uint8_t> ForwardToUpstream(const uint8_t* pkt, size_t len);
        std::string LookupCache(const std::string& key);
        void StoreCache(const std::string& key, const std::vector<uint8_t>& response, uint32_t ttl);

        std::string m_domain;
        EventBus* m_bus;
        int m_sockfd = -1;
        std::thread m_thread;
        std::atomic<bool> m_running{false};

        // Upstream resolvers
        std::vector<std::string> m_upstreams;

        // Simple cache: domain+type -> {response, expiry}
        struct CacheEntry {
            std::vector<uint8_t> data;
            std::chrono::steady_clock::time_point expires;
        };
        mutable std::mutex m_cache_mutex;
        std::unordered_map<std::string, CacheEntry> m_cache;

        std::atomic<int64_t> m_queries{0};
        std::atomic<int64_t> m_cache_hits{0};
    };
} // namespace chromelab

#endif // _LABD_DNS_SERVER_H_
