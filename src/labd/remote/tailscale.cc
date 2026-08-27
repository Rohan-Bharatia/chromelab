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

#include "labd/remote/tailscale.h"

namespace chromelab {
    TailscaleManager::TailscaleManager(EventBus* bus) :
        m_bus(bus) {}

    std::pair<int, std::string> TailscaleManager::Run(const std::string& cmd) {
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

    void TailscaleManager::Emit(const std::string& category_name, const std::string& message, int severity) const {
        if (!m_bus) {
            return;
        }

        Event ev;
        ev.set_category(EVENT_CATEGORY_NETWORK);
        ev.set_severity(static_cast<Severity>(severity));
        ev.set_source("tailscale");
        ev.set_message(category_name + ": " + message);
        m_bus->Emit(ev);
    }

    TSStatus TailscaleManager::GetStatus(void) const {
        TSStatus status;

        auto [rc, output] = Run("tailscale status --json");
        if (rc != 0) {
            status.set_state(TS_STATE_DOWN);
            status.set_backend_state("not-installed-or-down");
            status.set_error(output.empty() ? "tailscale CLI failed" : output);
            return status;
        }

        status.set_state(TS_STATE_UP);
        status.set_backend_state("running");

        // Grab hostname + DNS name
        auto [rc2, pretty] = Run("tailscale status");
        (void)rc2;
        if (!pretty.empty()) {
            // First non-empty line: "<ipv4>  <hostname>  <machine> ...
            // e.g. "100.101.102.103  chromelab      roro@  linux   -"
            std::istringstream stream(pretty);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty()) {
                    continue;
                }

                std::istringstream ls(line);
                std::string ip, hostname;
                ls >> ip >> hostname;
                if (!ip.empty() && ip[0] == '1' && ip.find('.') != std::string::npos) {
                    status.set_tailscale_ip(ip);
                    status.set_hostname(hostname);
                    break;
                }
            }
        }

        // DNS name via `tailscale ip -4` doesn't give name, so try status --json "DNSName"
        auto [rc3, dns] = Run("tailscale status --json | grep -o '\"DNSName[^,]*' | head -1");
        if (rc3 == 0 && !dns.empty()) {
            auto pos = dns.find(':');
            if (pos != std::string::npos) {
                std::string name = dns.substr(pos + 1);

                // Trim quotes
                if (!name.empty() && name.front() == '"') {
                    name.erase(name.begin());
                } if (!name.empty() && name.back() == '"') {
                    name.pop_back();
                }

                status.set_dns_name(name);
            }
        }

        // Machine ID
        auto [rc4, mid] = Run("tailscale status --json | grep -o '\"ID[^,]*' | head -1");
        if (rc4 == 0 && !mid.empty()) {
            auto pos = mid.find(':');
            if (pos != std::string::npos) {
                std::string id = mid.substr(pos + 1);

                if (!id.empty() && id.front() == '"') {
                    id.erase(id.begin());
                } if (!id.empty() && id.back() == '"') {
                    id.pop_back();
                }

                status.set_machine_id(id);
            }
        }

        return status;
    }

    TSStatus TailscaleManager::Up(const std::string& authkey) {
        TSStatus status;

        std::string cmd = "tailscale up";
        if (!authkey.empty()) {
            cmd += " --authkey " + authkey;
        }

        auto [rc, output] = Run(cmd);
        if (rc != 0) {
            status.set_state(TS_STATE_DOWN);
            status.set_error(output.empty() ? "tailscale up failed" : output);

            // Detect authentication requirement
            if (output.find("login") != std::string::npos || output.find("authenticate") != std::string::npos) {
                status.set_auth_required(true);
                status.set_backend_state("login-required");
            }

            return status;
        }

        // Verify state
        TSStatus st = GetStatus();
        st.set_state(TS_STATE_UP);
        return st;
    }

    TSStatus TailscaleManager::Down(void) {
        TSStatus status;

        auto [rc, output] = Run("tailscale down");
        if (rc != 0) {
            status.set_state(TS_STATE_UP);
            status.set_error(output.empty() ? "tailscale down failed" : output);
            return status;
        }

        status.set_state(TS_STATE_DOWN);
        return status;
    }

    TSStatus TailscaleManager::GetIP(void) const {
        TSStatus status = GetStatus();

        auto [rc, ip] = Run("tailscale ip -4");
        if (rc == 0 && !ip.empty() && ip != "no addresses") {
            status.set_tailscale_ip(ip);
        } else {
            status.set_error(ip);
        }

        return status;
    }

    bool TailscaleManager::IsUp(void) const {
        return GetStatus().state() == TS_STATE_UP;
    }
} // namespace chromelab
