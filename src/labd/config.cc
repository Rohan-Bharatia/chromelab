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

#include "labd/config.h"

namespace chromelab {
    template<typename T>
    static T GetOr(const toml::table& tbl, std::string_view key, T default_val) {
        if (auto node = tbl.get(key)) {
            if (auto val = node->value<T>()) {
                return *val;
            }
        }

        return default_val;
    }

    static std::string GetStr(const toml::table& tbl, std::string_view key, const std::string& default_val) {
        if (auto node = tbl.get(key)) {
            if (auto val = node->value<std::string>()) {
                return *val;
            }
        }

        return default_val;
    }

    static std::vector<std::string> GetStrArray(const toml::table& tbl, std::string_view key) {
        std::vector<std::string> result;
        if (auto node = tbl.get(key)) {
            if (auto arr = node->as_array()) {
                for (auto& item : *arr) {
                    if (auto s = item.value<std::string>()) {
                        result.push_back(*s);
                    }
                }
            }
        }

        return result;
    }

    bool LoadConfig(const std::string& path, LabdConfig& out, std::string& err_out) {
        try {
            toml::table tbl = toml::parse_file(path);

            // Daemon
            if (auto node = tbl.get("daemon")) {
                if (auto* daemon = node->as_table()) {
                    out.socket_path         = GetStr(*daemon, "socket", out.socket_path);
                    out.http_port           = GetOr<uint16_t>(*daemon, "http_port", out.http_port);
                    out.metrics_interval_ms = GetOr<int>(*daemon, "metrics_interval", out.metrics_interval_ms);
                    out.log_level           = GetStr(*daemon, "log_level", out.log_level);
                    out.log_file            = GetStr(*daemon, "log_file", out.log_file);
                    out.daemonize           = GetOr<bool>(*daemon, "daemonize", out.daemonize);
                    out.pid_file            = GetStr(*daemon, "pid_file", out.pid_file);
                }
            }

            // Paths
            if (auto node = tbl.get("paths")) {
                if (auto* paths = node->as_table()) {
                    out.web_dir     = GetStr(*paths, "web_dir", out.web_dir);
                    out.models_dir  = GetStr(*paths, "models_dir", out.models_dir);
                    out.events_dir  = GetStr(*paths, "events_dir", out.events_dir);
                }
            }

            // AI
            if (auto node = tbl.get("ai")) {
                if (auto* ai = node->as_table()) {
                    out.ai_enabled       = GetOr<bool>(*ai, "enabled", out.ai_enabled);
                    out.ai_default_model = GetStr(*ai, "default_model", out.ai_default_model);
                    out.ai_max_ram       = GetOr<int64_t>(*ai, "max_ram", out.ai_max_ram);
                }
            }

            // Wireguard
            if (auto node = tbl.get("wireguard")) {
                if (auto* wg = node->as_table()) {
                    out.wg_enabled    = GetOr<bool>(*wg, "enabled", out.wg_enabled);
                    out.wg_interface  = GetStr(*wg, "interface", out.wg_interface);
                    out.wg_listen_port = GetOr<uint16_t>(*wg, "listen_port", out.wg_listen_port);
                    out.wg_cidr       = GetStr(*wg, "cidr", out.wg_cidr);
                    out.wg_dns        = GetStr(*wg, "dns", out.wg_dns);
                }
            }

            // DNS
            if (auto node = tbl.get("dns")) {
                if (auto* dns = node->as_table()) {
                    out.dns_enabled = GetOr<bool>(*dns, "enabled", out.dns_enabled);
                    out.dns_domain  = GetStr(*dns, "domain", out.dns_domain);
                }
            }

            // Telemetry
            if (auto node = tbl.get("telemetry")) {
                if (auto* tel = node->as_table()) {
                    out.tel_cpu                = GetOr<bool>(*tel, "cpu", out.tel_cpu);
                    out.tel_memory             = GetOr<bool>(*tel, "memory", out.tel_memory);
                    out.tel_disk               = GetOr<bool>(*tel, "disk", out.tel_disk);
                    out.tel_network            = GetOr<bool>(*tel, "network", out.tel_network);
                    out.tel_temperature        = GetOr<bool>(*tel, "temperature", out.tel_temperature);
                    out.tel_load               = GetOr<bool>(*tel, "load", out.tel_load);
                    out.tel_processes          = GetOr<bool>(*tel, "processes", out.tel_processes);
                    out.tel_dns                = GetOr<bool>(*tel, "dns", out.tel_dns);
                    out.tel_uptime             = GetOr<bool>(*tel, "uptime", out.tel_uptime);
                    out.tel_network_interfaces = GetStrArray(*tel, "network_interfaces");
                    out.tel_disk_devices       = GetStrArray(*tel, "disk_devices");
                }
            }

            return true;

        } catch (const toml::parse_error& err) {
            std::ostringstream oss;
            oss << "TOML parse error in " << err.source().path
                << " at line " << err.source().begin.line
                << ", column " << err.source().begin.column
                << ": " << err.what();
            err_out = oss.str();
            return false;
        }
    }

    std::string ConfigToString(const LabdConfig& cfg) {
        std::ostringstream os;
        os << "[daemon]\n"
           << "  socket           = \"" << cfg.socket_path << "\"\n"
           << "  http_port        = " << cfg.http_port << "\n"
           << "  metrics_interval = " << cfg.metrics_interval_ms << "\n"
           << "  log_level        = \"" << cfg.log_level << "\"\n"
           << "  log_file         = \"" << cfg.log_file << "\"\n"
           << "  daemonize        = " << (cfg.daemonize ? "true" : "false") << "\n"
           << "  pid_file         = \"" << cfg.pid_file << "\"\n"
           << "\n"
           << "[paths]\n"
           << "  web_dir    = \"" << cfg.web_dir << "\"\n"
           << "  models_dir = \"" << cfg.models_dir << "\"\n"
           << "  events_dir = \"" << cfg.events_dir << "\"\n"
           << "\n"
           << "[ai]\n"
           << "  enabled        = " << (cfg.ai_enabled ? "true" : "false") << "\n"
           << "  default_model  = \"" << cfg.ai_default_model << "\"\n"
           << "  max_ram        = " << cfg.ai_max_ram << "\n"
           << "\n"
           << "[wireguard]\n"
           << "  enabled     = " << (cfg.wg_enabled ? "true" : "false") << "\n"
           << "  interface   = \"" << cfg.wg_interface << "\"\n"
           << "  listen_port = " << cfg.wg_listen_port << "\n"
           << "  cidr        = \"" << cfg.wg_cidr << "\"\n"
           << "  dns         = \"" << cfg.wg_dns << "\"\n"
           << "\n"
           << "[dns]\n"
           << "  enabled = " << (cfg.dns_enabled ? "true" : "false") << "\n"
           << "  domain  = \"" << cfg.dns_domain << "\"\n"
           << "\n"
           << "[telemetry]\n"
           << "  cpu         = " << (cfg.tel_cpu ? "true" : "false") << "\n"
           << "  memory      = " << (cfg.tel_memory ? "true" : "false") << "\n"
           << "  disk        = " << (cfg.tel_disk ? "true" : "false") << "\n"
           << "  network     = " << (cfg.tel_network ? "true" : "false") << "\n"
           << "  temperature = " << (cfg.tel_temperature ? "true" : "false") << "\n"
           << "  load        = " << (cfg.tel_load ? "true" : "false") << "\n"
           << "  processes   = " << (cfg.tel_processes ? "true" : "false") << "\n"
           << "  dns         = " << (cfg.tel_dns ? "true" : "false") << "\n"
           << "  uptime      = " << (cfg.tel_uptime ? "true" : "false") << "\n";
        return os.str();
    }
} // namespace chromelab
