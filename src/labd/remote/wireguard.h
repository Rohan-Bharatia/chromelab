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

#ifndef _LABD_REMOTE_WIREGUARD_H_
    #define _LABD_REMOTE_WIREGUARD_H_ (1)

#include "labd/events/bus.h"

namespace chromelab {
    // Manages WireGuard via wg/wg-quick commands
    class WireGuardManager {
    public:
        WireGuardManager(const std::string& interface, uint16_t listen_port, const std::string& cidr, const std::string& dns, EventBus* bus = nullptr);
        ~WireGuardManager(void) = default;

        WireGuardManager(const WireGuardManager&)            = delete;
        WireGuardManager& operator=(const WireGuardManager&) = delete;

        WGStatus GetStatus(void) const;
        WGStatus Up(void);
        WGStatus Down(void);
        WGStatus AddPeer(const std::string& name, const std::string& public_key, const std::string& allowed_ips, const std::string& endpoint);
        WGStatus RemovePeer(const std::string& public_key);
        std::vector<PeerInfo> ListPeers(void) const;

        static std::pair<std::string, std::string> GenerateKeypair(void);

    private:
        static std::pair<int, std::string> Run(const std::string& cmd);
        void WriteConfig(void);
        bool IsInterfaceUp(void) const;
        std::string GetPublicKey(void) const;

        std::string m_interface;
        uint16_t m_listen_port;
        std::string m_cidr;
        std::string m_dns;
        EventBus* m_bus;
    };
} // namespace chromelab

#endif // _LABD_REMOTE_WIREGUARD_H_
