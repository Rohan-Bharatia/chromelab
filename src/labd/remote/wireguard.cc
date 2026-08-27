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

#include "labd/remote/wireguard.h"

namespace chromelab {
    WireGuardManager::WireGuardManager(const std::string& interface, uint16_t listen_port, const std::string& cidr, const std::string& dns, EventBus* bus)
        : m_interface(interface),
        m_listen_port(listen_port),
        m_cidr(cidr),
        m_dns(dns),
        m_bus(bus) {}

    WGStatus WireGuardManager::GetStatus(void) const {
        WGStatus status;
        status.set_interface_name(m_interface);
        status.set_listen_port(std::to_string(m_listen_port));
        status.set_dns(m_dns);

        if (IsInterfaceUp()) {
            status.set_state(WG_STATE_UP);
            status.set_public_key(GetPublicKey());

            auto [rc, output] = Run("wg show " + m_interface + " dump");
            if (rc == 0) {
                std::istringstream stream(output);
                std::string line;
                std::getline(stream, line);
                while (std::getline(stream, line)) {
                    std::istringstream ls(line);
                    std::string pk, preshared, endpoint, allowed, handshake, rx, tx, keep;
                    ls >> pk >> preshared >> endpoint >> allowed >> handshake >> rx >> tx >> keep;

                    PeerInfo* peer = status.add_peers();
                    peer->set_public_key(pk);
                    peer->set_endpoint(endpoint);
                    peer->set_allowed_ips(allowed);
                    peer->set_latest_handshake_ms(handshake == "0" ? 0 : std::stoll(handshake));
                    peer->set_transfer_rx_bytes(std::stoll(rx));
                    peer->set_transfer_tx_bytes(std::stoll(tx));
                }
            }
        } else {
            status.set_state(WG_STATE_DOWN);
        }

        return status;
    }

    WGStatus WireGuardManager::Up(void) {
        WGStatus status;
        status.set_interface_name(m_interface);

        WriteConfig();

        auto [rc, output] = Run("wg-quick up " + m_interface);
        if (rc != 0) {
            status.set_state(WG_STATE_DOWN);
            status.set_error(output.empty() ? "wg-quick up failed" : output);
            return status;
        }

        status.set_state(WG_STATE_UP);
        status.set_public_key(GetPublicKey());

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_NETWORK);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("wireguard");
            ev.set_message("WireGuard up: " + m_interface);
            m_bus->Emit(ev);
        }

        return status;
    }

    WGStatus WireGuardManager::Down(void) {
        WGStatus status;
        status.set_interface_name(m_interface);

        auto [rc, output] = Run("wg-quick down " + m_interface);
        if (rc != 0) {
            status.set_state(WG_STATE_UP);
            status.set_error(output.empty() ? "wg-quick down failed" : output);
            return status;
        }

        status.set_state(WG_STATE_DOWN);

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_NETWORK);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("wireguard");
            ev.set_message("WireGuard down: " + m_interface);
            m_bus->Emit(ev);
        }

        return status;
    }

    WGStatus WireGuardManager::AddPeer(const std::string& name, const std::string& public_key, const std::string& allowed_ips, const std::string& endpoint) {
        WGStatus status;
        status.set_interface_name(m_interface);

        std::string cmd = "wg set " + m_interface + " peer " + public_key;
        if (!endpoint.empty()) {
            cmd += " endpoint " + endpoint;
        } if (!allowed_ips.empty()) {
            cmd += " allowed-ips " + allowed_ips;
        }

        auto [rc, output] = Run(cmd);
        if (rc != 0) {
            status.set_state(IsInterfaceUp() ? WG_STATE_UP : WG_STATE_DOWN);
            status.set_error(output.empty() ? "add peer failed" : output);
            return status;
        }

        WriteConfig();

        status.set_state(WG_STATE_UP);
        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_SECURITY);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("wireguard");
            ev.set_message("Peer added: " + name + " (" + public_key.substr(0, 8) + "...)");
            m_bus->Emit(ev);
        }

        return status;
    }

    WGStatus WireGuardManager::RemovePeer(const std::string& public_key) {
        WGStatus status;
        status.set_interface_name(m_interface);

        auto [rc, output] = Run("wg set " + m_interface + " peer " + public_key + " remove");
        if (rc != 0) {
            status.set_state(IsInterfaceUp() ? WG_STATE_UP : WG_STATE_DOWN);
            status.set_error(output.empty() ? "remove peer failed" : output);
            return status;
        }

        WriteConfig();
        status.set_state(IsInterfaceUp() ? WG_STATE_UP : WG_STATE_DOWN);

        if (m_bus) {
            Event ev;
            ev.set_category(EVENT_CATEGORY_SECURITY);
            ev.set_severity(SEVERITY_INFO);
            ev.set_source("wireguard");
            ev.set_message("Peer removed: " + public_key.substr(0, 8) + "...");
            m_bus->Emit(ev);
        }

        return status;
    }

    std::vector<PeerInfo> WireGuardManager::ListPeers(void) const {
        std::vector<PeerInfo> result;

        auto [rc, output] = Run("wg show " + m_interface + " dump");
        if (rc != 0) return result;

        std::istringstream stream(output);
        std::string line;
        std::getline(stream, line);
        while (std::getline(stream, line)) {
            std::istringstream ls(line);
            std::string pk, psk, ep, allowed, hs, rx, tx, keep;
            ls >> pk >> psk >> ep >> allowed >> hs >> rx >> tx >> keep;

            PeerInfo p;
            p.set_public_key(pk);
            p.set_endpoint(ep);
            p.set_allowed_ips(allowed);
            p.set_latest_handshake_ms(hs == "0" ? 0 : std::stoll(hs));
            p.set_transfer_rx_bytes(std::stoll(rx));
            p.set_transfer_tx_bytes(std::stoll(tx));
            result.push_back(std::move(p));
        }

        return result;
    }

    std::pair<std::string, std::string> WireGuardManager::GenerateKeypair(void) {
        auto [rc1, privkey] = Run("wg genkey");
        if (rc1 != 0 || privkey.empty()) {
            return { "", "" };
        }

        auto [rc2, pubkey] = Run("echo '" + privkey + "' | wg pubkey");
        if (rc2 != 0 || pubkey.empty()) {
            return { privkey, "" };
        }

        return { privkey, pubkey };
    }

    std::pair<int, std::string> WireGuardManager::Run(const std::string& cmd) {
        std::string combined;
        std::array<char, 256> buf{};
        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
        if (!pipe) {
            return { -1, "popen failed" };
        }
        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
            combined += buf.data();
        }
        int rc = pclose(pipe);
        while (!combined.empty() && (combined.back() == '\n' || combined.back() == '\r' || combined.back() == ' ')) {
            combined.pop_back();
        }
        return { WEXITSTATUS(rc), combined };
    }

    void WireGuardManager::WriteConfig(void) {
        std::string path = "/etc/wireguard/" + m_interface + ".conf";
        std::error_code ec;
        std::filesystem::create_directories("/etc/wireguard", ec);

        std::ofstream conf(path);
        if (!conf.is_open()) {
            return;
        }

        conf << "[Interface]\n";
        conf << "ListenPort = " << m_listen_port << "\n";
        conf << "Address = " << m_cidr << "\n";
        if (!m_dns.empty()) {
            conf << "DNS = " << m_dns << "\n";
        }
        conf << "\n";

        auto [rc, output] = Run("wg show " + m_interface + " dump");
        if (rc == 0) {
            std::istringstream stream(output);
            std::string line;
            std::getline(stream, line);
            while (std::getline(stream, line)) {
                std::istringstream ls(line);
                std::string pk, psk, ep, allowed, hs, rx, tx, keep;
                ls >> pk >> psk >> ep >> allowed >> hs >> rx >> tx >> keep;
                conf << "[Peer]\n";
                conf << "PublicKey = " << pk << "\n";
                if (!psk.empty() && psk != "(none)") {
                    conf << "PresharedKey = " << psk << "\n";
                } if (!allowed.empty() && allowed != "(none)") {
                    conf << "AllowedIPs = " << allowed << "\n";
                } if (!ep.empty() && ep != "(none)") {
                    conf << "Endpoint = " << ep << "\n";
                }
                conf << "\n";
            }
        }
    }

    bool WireGuardManager::IsInterfaceUp(void) const {
        auto [rc, output] = Run("wg show " + m_interface);
        return rc == 0 && !output.empty() && output.find("interface") != std::string::npos;
    }

    std::string WireGuardManager::GetPublicKey(void) const {
        auto [rc, output] = Run("wg show " + m_interface + " public-key");
        if (rc != 0 || output.empty()) {
            auto [rc2, pk] = Run("wg pubkey < /etc/wireguard/" + m_interface + ".key");
            return (rc2 == 0) ? pk : "";
        }

        return output;
    }
} // namespace chromelab
