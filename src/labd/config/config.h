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

#ifndef _LABD_CONFIG_H_
    #define _LABD_CONFIG_H_ (1)

#include "pch.h"

namespace chromelab {
    struct LabdConfig {
        // Daemon
        std::string socket_path = "/run/chromelab/labd.sock";
        uint16_t http_port      = 8080;
        int metrics_interval_ms = 2000;
        std::string log_level   = "info";
        std::string log_file    = "/var/log/chromelab/labd.log";
        bool daemonize          = false;
        std::string pid_file    = "/run/chromelab/labd.pid";

        // Paths
        std::string web_dir    = "/usr/share/chromelab/web";
        std::string models_dir = "/var/lib/chromelab/models";
        std::string events_dir = "/var/lib/chromelab/events";

        // AI
        bool ai_enabled              = false;
        std::string ai_default_model = "";
        int64_t ai_max_ram           = 1073741824; // 1 GB

        // Wireguard
        bool wg_enabled          = false;
        std::string wg_interface = "wg0";
        uint16_t wg_listen_port  = 51820;
        std::string wg_cidr      = "10.0.0.0/24";
        std::string wg_dns       = "1.1.1.1";

        // Tailscale
        bool ts_enabled        = true;
        std::string ts_authkey = "";

        // DNS
        bool dns_enabled       = false;
        std::string dns_domain = "lab";

        // Telemetry
        bool tel_cpu         = true;
        bool tel_memory      = true;
        bool tel_disk        = true;
        bool tel_network     = true;
        bool tel_temperature = true;
        bool tel_load        = true;
        bool tel_processes   = true;
        bool tel_dns         = true;
        bool tel_uptime      = true;
        std::vector<std::string> tel_network_interfaces;
        std::vector<std::string> tel_disk_devices;
    };

    bool LoadConfig(const std::string& path, LabdConfig& out, std::string& err_out);
    std::string ConfigToString(const LabdConfig& cfg);
} // namespace chromelab

#endif // _LABD_CONFIG_H_
