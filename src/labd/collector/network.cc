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

#include "labd/collector/network.h"

namespace chromelab {
    NetworkCollector::NetworkCollector(const std::vector<std::string>& iface_filter)
        : m_filter(iface_filter) {}

    static int count_connections(const char* path, const char* state_filter) {
        std::ifstream f(path);
        if (!f.is_open()) {
            return 0;
        }

        int count = 0;
        std::string line;
        // Skip header lines
        std::getline(f, line); // header 1
        std::getline(f, line); // header 2

        while (std::getline(f, line)) {
            std::istringstream iss(line);
            // local_address remote_address st ...
            std::string local, remote, state;
            iss >> local >> remote >> state;

            // State is hex: 01=ESTABLISHED, 06=TIME_WAIT, etc.
            if (state_filter) {
                if (state != state_filter) continue;
            }
            ++count;
        }

        return count;
    }

    void NetworkCollector::Collect(MetricSnapshot& snap) {
        NetworkMetrics* net = snap.mutable_network();

        // Parse /proc/net/dev for interface counters
        std::ifstream dev("/proc/net/dev");
        if (dev.is_open()) {
            std::string line;
            // Skip 2 header lines
            std::getline(dev, line);
            std::getline(dev, line);

            while (std::getline(dev, line)) {
                // Format: "  iface: rx_bytes rx_packets rx_errs rx_drop ... tx_bytes tx_packets tx_errs tx_drop ..."
                size_t colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }

                std::string name = line.substr(0, colon); // Trim leading whitespace
                size_t start     = name.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    name = name.substr(start);
                }

                // Skip loopback
                if (name == "lo") {
                    continue;
                }

                // Apply filter
                if (!m_filter.empty()) {
                    if (std::find(m_filter.begin(), m_filter.end(), name) == m_filter.end()) continue;
                }

                std::istringstream iss(line.substr(colon + 1));
                int64_t rx_bytes, rx_packets, rx_errs, rx_drop;
                int64_t dummy1, dummy2, dummy3;
                int64_t tx_bytes, tx_packets, tx_errs, tx_drop;
                int64_t td1, td2, td3;

                iss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> dummy1 >> dummy2 >> dummy3
                    >> tx_bytes >> tx_packets >> tx_errs >> tx_drop >> td1 >> td2 >> td3;

                auto* iface = net->add_interfaces();
                iface->set_name(name);
                iface->set_rx_bytes(rx_bytes);
                iface->set_rx_packets(rx_packets);
                iface->set_rx_errors(rx_errs);
                iface->set_rx_dropped(rx_drop);
                iface->set_tx_bytes(tx_bytes);
                iface->set_tx_packets(tx_packets);
                iface->set_tx_errors(tx_errs);
                iface->set_tx_dropped(tx_drop);
            }
        }

        // Connection counts from /proc/net/tcp and udp
        net->set_tcp_established(count_connections("/proc/net/tcp", "01"));
        net->set_tcp_time_wait(count_connections("/proc/net/tcp", "06"));
        net->set_udp_connections(count_connections("/proc/net/udp", "07"));
    }
} // namespace chromelab
