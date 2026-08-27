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

#include "labd/dns/server.h"

namespace chromelab {
    DnsServer::DnsServer(const std::string& domain, EventBus* bus)
        : m_domain(domain),
        m_bus(bus) {
        // Read upstream resolvers from /etc/resolv.conf
        std::ifstream resolv("/etc/resolv.conf");
        std::string line;
        while (std::getline(resolv, line)) {
            if (line.compare(0, 8, "nameserver") != 0) {
                continue;
            }

            auto pos = line.find(' ');
            if (pos == std::string::npos) {
                continue;
            }
            std::string ns = line.substr(pos + 1);

            // Trim
            while (!ns.empty() && ns.back() == '\n') {
                ns.pop_back();
            }
            if (!ns.empty()) {
                m_upstreams.push_back(ns);
            }
        }
        if (m_upstreams.empty()) {
            m_upstreams.push_back("8.8.8.8");
            m_upstreams.push_back("1.1.1.1");
        }
    }

    DnsServer::~DnsServer(void) {
        Stop();
    }

    bool DnsServer::Start(uint16_t port) {
        m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sockfd < 0) {
            return false;
        }

        int opt = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(port);

        if (bind(m_sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(m_sockfd);
            m_sockfd = -1;
            return false;
        }

        m_running = true;
        m_thread  = std::thread(&DnsServer::RecvLoop, this);

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_DNS);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("dns-server");
            ev.set_message("DNS server started on port " + std::to_string(port));
            m_bus->Emit(ev);
        }

        return true;
    }

    void DnsServer::Stop(void) {
        m_running.store(false);
        if (m_sockfd >= 0) {
            close(m_sockfd);
            m_sockfd = -1;
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool DnsServer::IsRunning(void) const {
        return m_running.load();
    }

    int64_t DnsServer::QueriesTotal(void) const {
        return m_queries.load();
    }

    int64_t DnsServer::CacheHits(void) const {
        return m_cache_hits.load();
    }

    void DnsServer::RecvLoop(void) {
        uint8_t buf[1024];

        while (m_running.load()) {
            struct sockaddr_in client{};
            socklen_t client_len = sizeof(client);

            // Use select for timeout so we can check m_running
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_sockfd, &fds);

            struct timeval tv{};
            tv.tv_sec  = 1;
            tv.tv_usec = 0;

            int sel = select(m_sockfd + 1, &fds, nullptr, nullptr, &tv);
            if (sel <= 0) {
                continue;
            }

            // Minimum DNS header size
            ssize_t n = recvfrom(m_sockfd, buf, sizeof(buf), 0, reinterpret_cast<struct sockaddr*>(&client), &client_len);
            if (n < 12) {
                continue;
            }

            ++m_queries;

            // Parse query name for cache key
            std::string qname;
            size_t pos = 12;
            while (pos < static_cast<size_t>(n) && buf[pos] != 0) {
                uint8_t label_len = buf[pos++];
                if (pos + label_len > static_cast<size_t>(n)) {
                    break;
                } if (!qname.empty()) {
                    qname += ".";
                }
                qname.append(reinterpret_cast<char*>(buf + pos), label_len);
                pos += label_len;
            }

            uint16_t qtype = 0;
            if (pos + 2 <= static_cast<size_t>(n)) {
                qtype = (buf[pos] << 8) | buf[pos + 1];
            }

            std::string cache_key = qname + ":" + std::to_string(qtype);

            // Check cache
            {
                std::lock_guard lock(m_cache_mutex);
                auto it = m_cache.find(cache_key);
                if (it != m_cache.end() && std::chrono::steady_clock::now() < it->second.expires) {
                    ++m_cache_hits;

                    // Rewrite transaction ID from original query
                    auto resp = it->second.data;
                    resp[0]   = buf[0];
                    resp[1]   = buf[1];

                    sendto(m_sockfd, resp.data(), resp.size(), 0, reinterpret_cast<struct sockaddr*>(&client), client_len);
                    continue;
                }
            }

            // Forward to upstream
            auto resp = ForwardToUpstream(buf, static_cast<size_t>(n));
            if (resp.empty()) {
                // Send SERVFAIL
                std::vector<uint8_t> servfail(12, 0);
                servfail[0] = buf[0]; servfail[1] = buf[1];
                servfail[2] = 0x81; servfail[3] = 0x82; // flags: QR+RCODE=SERVFAIL
                servfail[5] = 1; // QDCOUNT

                sendto(m_sockfd, servfail.data(), servfail.size(), 0, reinterpret_cast<struct sockaddr*>(&client), client_len);
                continue;
            }

            // Extract TTL from answer section and cache
            uint32_t ttl     = 300; // default 5 min
            size_t ans_start = 12;
            // Skip question
            while (ans_start < resp.size() && resp[ans_start] != 0) {
                ++ans_start;
            }
            ans_start += 5; // null + qtype(2) + qclass(2)

            if (ans_start + 12 <= resp.size()) {
                ttl = (resp[ans_start + 6] << 24) | (resp[ans_start + 7] << 16) |
                      (resp[ans_start + 8] << 8)  | resp[ans_start + 9];

                // Cap at 24h
                if (ttl > 86400) {
                    ttl = 86400;
                }
            }

            StoreCache(cache_key, resp, ttl);

            sendto(m_sockfd, resp.data(), resp.size(), 0, reinterpret_cast<struct sockaddr*>(&client), client_len);
        }
    }

    std::vector<uint8_t> DnsServer::ForwardToUpstream(const uint8_t* pkt, size_t len) {
        for (const auto& upstream : m_upstreams) {
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0) {
                continue;
            }

            struct timeval tv{};
            tv.tv_sec  = 3;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            struct sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port   = htons(53);
            inet_pton(AF_INET, upstream.c_str(), &dest.sin_addr);

            sendto(fd, pkt, len, 0, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));

            uint8_t resp_buf[1024];
            ssize_t n = recvfrom(fd, resp_buf, sizeof(resp_buf), 0, nullptr, nullptr);
            close(fd);

            if (n >= 12) {
                return std::vector<uint8_t>(resp_buf, resp_buf + n);
            }
        }

        return {};
    }

    void DnsServer::StoreCache(const std::string& key, const std::vector<uint8_t>& response, uint32_t ttl) {
        std::lock_guard lock(m_cache_mutex);
        CacheEntry entry;
        entry.data    = response;
        entry.expires = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);
        m_cache[key]  = std::move(entry);
    }
} // namespace chromelab
