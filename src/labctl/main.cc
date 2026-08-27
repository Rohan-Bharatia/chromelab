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

#include "labctl/tui.h"
#include "labd/remote/wireguard.h"

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <command> [options]\n\n"
              << "Commands:\n"
              << "  status              Show system status\n"
              << "  info                Show system information\n"
              << "  metrics             Show current metrics\n"
              << "  metrics --watch     Stream metrics live\n"
              << "  events              List recent events\n"
              << "  events --tail       Stream events live\n"
              << "  svc list            List services\n"
              << "  svc info <name>     Show service info\n"
              << "  svc start <name>    Start a service\n"
              << "  svc stop <name>     Stop a service\n"
              << "  svc restart <name>  Restart a service\n"
              << "  svc enable <name>   Enable a service\n"
              << "  svc disable <name>  Disable a service\n"
              << "  svc health <name>   Check service health\n"
              << "  ai status           AI module status\n"
              << "  ai models           List available models\n"
              << "  ai load <model>     Load a model\n"
              << "  ai unload           Unload current model\n"
              << "  ai chat <prompt>    Chat with loaded model\n"
              << "  wg status           WireGuard status\n"
              << "  wg up               Bring WireGuard up\n"
              << "  wg down             Bring WireGuard down\n"
              << "  wg peers            List WireGuard peers\n"
              << "  wg add-peer         Add a peer (interactive)\n"
              << "  wg remove-peer <pk> Remove a peer by public key\n"
              << "  wg genkey           Generate a WireGuard keypair\n"
              << "  config validate     Validate configuration\n"
              << "  tui                 Live terminal dashboard\n"
              << "\nOptions:\n"
              << "  -s, --socket PATH   Daemon socket (default: /run/chromelab/labd.sock)\n"
              << "  -j, --json          Output as JSON\n"
              << "  -h, --help          Show this help\n";
}

static std::shared_ptr<grpc::Channel> connect_daemon(const std::string& socket_path) {
    return grpc::CreateChannel("unix:" + socket_path, grpc::InsecureChannelCredentials());
}

static int cmd_status(const std::shared_ptr<grpc::Channel>& channel) {
    auto stub = chromelab::LabDaemon::NewStub(channel);
    grpc::ClientContext ctx;
    chromelab::Empty req;
    chromelab::StatusResponse resp;

    auto status = stub->GetStatus(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "Error: " << status.error_message() << "\n";
        return 1;
    }

    std::cout << "labd v" << resp.version() << "\n"
              << "  Hostname:     " << resp.hostname() << "\n"
              << "  Uptime:       " << resp.uptime_seconds() << "s\n"
              << "  AI loaded:    " << (resp.ai_loaded() ? "yes" : "no") << "\n"
              << "  WireGuard:    " << (resp.wireguard_active() ? "active" : "inactive") << "\n"
              << "  Services:     " << resp.services_running() << "/" << resp.services_total() << " running\n";
    return 0;
}

static int cmd_info(const std::shared_ptr<grpc::Channel>& channel) {
    auto stub = chromelab::LabDaemon::NewStub(channel);
    grpc::ClientContext ctx;
    chromelab::Empty req;
    chromelab::SystemInfo resp;

    auto status = stub->GetSystemInfo(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "Error: " << status.error_message() << "\n";
        return 1;
    }

    std::cout << "System Information\n"
              << "  Hostname:      " << resp.hostname() << "\n"
              << "  Kernel:        " << resp.kernel_version() << "\n"
              << "  Alpine:        " << resp.alpine_version() << "\n"
              << "  Arch:          " << resp.arch() << "\n"
              << "  CPU:           " << resp.cpu_model() << " (" << resp.cpu_cores() << " cores)\n"
              << "  RAM:           " << resp.total_ram_bytes() / (1024 * 1024) << " MB\n"
              << "  Disk:          " << resp.total_disk_bytes() / (1024 * 1024 * 1024) << " GB\n";
    return 0;
}

static void print_snapshot(const chromelab::MetricSnapshot& snap) {
    const auto& cpu = snap.cpu();
    std::cout << "CPU:       " << cpu.overall_percent() << "% (" << cpu.cores_size() << " cores)\n";
    for (int i = 0; i < cpu.cores_size(); ++i) {
        const auto& c = cpu.cores(i);
        std::cout << "  core " << c.core_id() << ":  " << c.percent() << "%\n";
    }

    const auto& mem = snap.memory();
    if (mem.total_bytes() > 0) {
        std::cout << "Memory:    " << mem.used_bytes() / (1024 * 1024) << " / "
                  << mem.total_bytes() / (1024 * 1024) << " MB (" << mem.percent() << "%)\n";
        if (mem.swap_total_bytes() > 0) {
            std::cout << "Swap:      " << mem.swap_used_bytes() / (1024 * 1024) << " / "
                      << mem.swap_total_bytes() / (1024 * 1024) << " MB (" << mem.swap_percent() << "%)\n";
        }
    }

    const auto& load = snap.load();
    std::cout << "Load:      " << load.load_1m() << " " << load.load_5m() << " " << load.load_15m() << "\n";

    const auto& disk = snap.disk();
    for (int i = 0; i < disk.filesystems_size(); ++i) {
        const auto& fs = disk.filesystems(i);
        std::cout << "Disk " << fs.mount_point() << ":  "
                  << fs.used_bytes() / (1024 * 1024 * 1024) << " / "
                  << fs.total_bytes() / (1024 * 1024 * 1024) << " GB (" << fs.percent() << "%)\n";
    }

    const auto& net = snap.network();
    for (int i = 0; i < net.interfaces_size(); ++i) {
        const auto& iface = net.interfaces(i);
        std::cout << "Net " << iface.name() << ":   rx="
                  << iface.rx_bytes() / 1024 << "KB tx="
                  << iface.tx_bytes() / 1024 << "KB"
                  << " pkts=" << iface.rx_packets() << "/" << iface.tx_packets() << "\n";
    }
    std::cout << "Connections: TCP=" << net.tcp_established() << " est, "
              << net.tcp_time_wait() << " tw | UDP=" << net.udp_connections() << "\n";

    const auto& temp = snap.temperature();
    for (int i = 0; i < temp.zones_size(); ++i) {
        const auto& z = temp.zones(i);
        std::cout << "Temp " << z.name() << ":    " << z.temp_celsius() << " C (" << z.type() << ")\n";
    }

    const auto& procs = snap.processes();
    std::cout << "Procs:     " << procs.total() << " total, "
              << procs.running() << " run, "
              << procs.sleeping() << " sleep, "
              << procs.zombie() << " zombie\n";

    const auto& up = snap.uptime();
    std::cout << "Uptime:    " << up.uptime_human() << "\n";
}

static int cmd_metrics(const std::shared_ptr<grpc::Channel>& channel, bool watch) {
    auto stub = chromelab::LabDaemon::NewStub(channel);

    if (watch) {
        grpc::ClientContext ctx;
        chromelab::StreamRequest req;
        req.set_interval_ms(2000);
        auto reader = stub->StreamMetrics(&ctx, req);

        chromelab::MetricSnapshot snap;
        while (reader->Read(&snap)) {
            std::cout << "\033[2J\033[H";
            std::cout << "=== chromelab metrics (live) ===\n\n";
            print_snapshot(snap);
            std::cout << std::flush;
        }

        return 0;
    }

    grpc::ClientContext ctx;
    chromelab::MetricsRequest req;
    chromelab::MetricSnapshot resp;

    auto status = stub->GetMetrics(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "Error: " << status.error_message() << "\n";
        return 1;
    }

    std::cout << "=== chromelab metrics ===\n\n";
    print_snapshot(resp);
    return 0;
}

static const char* severity_str(chromelab::Severity sev) {
    switch (sev) {
        case chromelab::SEVERITY_INFO:
            return "INFO";
        case chromelab::SEVERITY_WARNING:
            return "WARN";
        case chromelab::SEVERITY_ERROR:
            return "ERR";
        case chromelab::SEVERITY_CRITICAL:
            return "CRIT";
        default:
            return "---";
    }
}

static const char* category_str(chromelab::EventCategory cat) {
    switch (cat) {
        case chromelab::EVENT_CATEGORY_SYSTEM:
            return "system";
        case chromelab::EVENT_CATEGORY_SERVICE:
            return "service";
        case chromelab::EVENT_CATEGORY_NETWORK:
            return "network";
        case chromelab::EVENT_CATEGORY_AI:
            return "ai";
        case chromelab::EVENT_CATEGORY_SECURITY:
            return "security";
        case chromelab::EVENT_CATEGORY_DISK:
            return "disk";
        case chromelab::EVENT_CATEGORY_TEMPERATURE:
            return "temp";
        case chromelab::EVENT_CATEGORY_DNS:
            return "dns";
        default:
            return "other";
    }
}

static int cmd_events(const std::shared_ptr<grpc::Channel>& channel, bool tail) {
    auto stub = chromelab::LabDaemon::NewStub(channel);

    if (tail) {
        grpc::ClientContext ctx;
        chromelab::StreamRequest req;
        req.set_interval_ms(0);
        auto reader = stub->StreamEvents(&ctx, req);

        chromelab::Event event;
        while (reader->Read(&event)) {
            auto ts     = std::chrono::system_clock::time_point(std::chrono::milliseconds(event.timestamp_ms()));
            auto time_t = std::chrono::system_clock::to_time_t(ts);
            char buf[20];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time_t));

            std::cout << buf << "  "
                      << severity_str(event.severity()) << "  "
                      << category_str(event.category()) << "  "
                      << event.source() << ": "
                      << event.message() << "\n" << std::flush;
        }

        return 0;
    }

    grpc::ClientContext ctx;
    chromelab::EventsRequest req;
    req.set_limit(50);
    chromelab::EventsResponse resp;

    auto status = stub->ListEvents(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "Error: " << status.error_message() << "\n";
        return 1;
    }

    if (resp.events_size() == 0) {
        std::cout << "No events recorded.\n";
        return 0;
    }

    for (const auto& ev : resp.events()) {
        auto ts     = std::chrono::system_clock::time_point(std::chrono::milliseconds(ev.timestamp_ms()));
        auto time_t = std::chrono::system_clock::to_time_t(ts);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time_t));

        std::cout << buf << "  "
                  << severity_str(ev.severity()) << "  "
                  << category_str(ev.category()) << "  "
                  << ev.source() << ": "
                  << ev.message() << "\n";
    }

    return 0;
}

static const char* svc_state_str(chromelab::ServiceState s) {
    switch (s) {
        case chromelab::SERVICE_STATE_RUNNING:
            return "running";
        case chromelab::SERVICE_STATE_STOPPED:
            return "stopped";
        case chromelab::SERVICE_STATE_STARTING:
            return "starting";
        case chromelab::SERVICE_STATE_STOPPING:
            return "stopping";
        case chromelab::SERVICE_STATE_FAILED:
            return "failed";
        default:
            return "unknown";
    }
}

static int cmd_svc(const std::shared_ptr<grpc::Channel>& channel, const std::vector<std::string>& args, int start) {
    if (start >= static_cast<int>(args.size())) {
        std::cerr << "Usage: labctl svc <list|start|stop|restart|enable|disable> [name]\n";
        return 1;
    }

    auto stub          = chromelab::LabDaemon::NewStub(channel);
    const auto& action = args[start];

    if (action == "list") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::ServicesList resp;

        auto status = stub->ListServices(&ctx, req, &resp);
        if (!status.ok()) {
            std::cerr << "Error: " << status.error_message() << "\n";
            return 1;
        }

        if (resp.services_size() == 0) {
            std::cout << "No services found.\n";
            return 0;
        }

        std::cout << "NAME                         STATE      ENABLED\n";
        std::cout << "─────────────────────────────────────────────────\n";
        for (const auto& svc : resp.services()) {
            std::cout << svc.name();

            for (int p = svc.name().size(); p < 29; ++p) {
                std::cout << ' ';
            }

            std::cout << svc_state_str(svc.state());

            std::string s = svc_state_str(svc.state());
            for (int p = s.size(); p < 11; ++p) {
                std::cout << ' ';
            }

            std::cout << (svc.enabled() ? "yes" : "no") << "\n";
        }
        return 0;
    }

    if (start + 1 >= static_cast<int>(args.size())) {
        std::cerr << "Service name required\n";
        return 1;
    }

    chromelab::ServiceRequest req;
    req.set_name(args[start + 1]);
    grpc::ClientContext ctx;

    if (action == "info") {
        chromelab::ServiceInfo info;
        auto status = stub->GetService(&ctx, req, &info);
        if (!status.ok()) {
            std::cerr << "Error: " << status.error_message() << "\n";
            return 1;
        }

        std::cout << "Service: " << info.name() << "\n"
                  << "  State:   " << svc_state_str(info.state()) << "\n"
                  << "  Enabled: " << (info.enabled() ? "yes" : "no") << "\n"
                  << "  Runlevel: " << info.runlevel() << "\n";

        return 0;
    }

    chromelab::ServiceResponse resp;
    grpc::Status status;
    if (action == "start") {
        status = stub->StartService(&ctx, req, &resp);
    } else if (action == "stop") {
        status = stub->StopService(&ctx, req, &resp);
    } else if (action == "restart") {
        status = stub->RestartService(&ctx, req, &resp);
    } else if (action == "enable") {
        status = stub->EnableService(&ctx, req, &resp);
    } else if (action == "disable") {
        status = stub->DisableService(&ctx, req, &resp);
    } else if (action == "health") {
        chromelab::HealthStatus health;
        auto s = stub->CheckServiceHealth(&ctx, req, &health);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << health.service_name() << ": " << (health.healthy() ? "healthy" : "unhealthy")
                  << " (" << health.message() << ")\n";

        return 0;
    } else {
        std::cerr << "Unknown action: " << action << "\n";
        std::cerr << "Usage: labctl svc <list|info|start|stop|restart|enable|disable|health> [name]\n";
        return 1;
    }

    if (!status.ok()) {
        std::cerr << "Error: " << status.error_message() << "\n";
        return 1;
    }

    if (!resp.error().empty()) {
        std::cerr << "Error: " << resp.error() << "\n";
        return 1;
    }

    std::cout << resp.name() << ": " << svc_state_str(resp.current_state()) << "\n";
    return 0;
}

static const char* model_status_str(chromelab::ModelStatus s) {
    switch (s) {
        case chromelab::MODEL_STATUS_UNLOADED:
            return "unloaded";
        case chromelab::MODEL_STATUS_LOADING:
            return "loading";
        case chromelab::MODEL_STATUS_READY:
            return "ready";
        case chromelab::MODEL_STATUS_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static int cmd_ai(const std::shared_ptr<grpc::Channel>& channel, const std::vector<std::string>& args, int start) {
    if (start >= static_cast<int>(args.size())) {
        std::cerr << "Usage: labctl ai <status|models|load|unload|chat> [args]\n";
        return 1;
    }

    auto stub          = chromelab::LabDaemon::NewStub(channel);
    const auto& action = args[start];

    if (action == "status") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::AIStatus resp;
        auto s = stub->GetAIStatus(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << "AI: " << model_status_str(resp.status()) << "\n"
                  << "  Model:  " << (resp.current_model().empty() ? "(none)" : resp.current_model()) << "\n"
                  << "  Error:  " << (resp.error().empty() ? "-" : resp.error()) << "\n";
        return 0;
    }

    if (action == "models") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::ModelsList resp;
        auto s = stub->ListModels(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }
        if (resp.models_size() == 0) {
            std::cout << "No models found in models directory.\n";
            return 0;
        }

        for (const auto& m : resp.models()) {
            std::cout << (m.loaded() ? "* " : "  ") << m.name();
            if (!m.quantization().empty()) {
                std::cout << " [" << m.quantization() << "]";
            }

            if (m.size_bytes() > 0) {
                std::cout << "  " << m.size_bytes() / (1024*1024) << " MB";
            }

            std::cout << "\n";
        }
        return 0;
    }

    if (action == "load") {
        if (start + 1 >= static_cast<int>(args.size())) {
            std::cerr << "Model name required\n";
            return 1;
        }

        grpc::ClientContext ctx;
        chromelab::ModelRequest req;
        req.set_model_name(args[start + 1]);
        chromelab::AIStatus resp;
        auto s = stub->LoadModel(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << "Load: " << model_status_str(resp.status());
        if (!resp.error().empty()) {
            std::cout << " (" << resp.error() << ")";
        }

        std::cout << "\n";
        return 0;
    }

    if (action == "unload") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::AIStatus resp;
        auto s = stub->UnloadModel(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << "Unloaded.\n";
        return 0;
    }

    if (action == "chat") {
        std::string prompt;
        for (int i = start + 1; i < static_cast<int>(args.size()); i++) {
            if (i > start + 1) {
                prompt += " ";
            }
            prompt += args[i];
        }
        if (prompt.empty()) {
            std::cerr << "Prompt required\n";
            return 1;
        }

        grpc::ClientContext ctx;
        chromelab::ChatRequest req;
        auto* msg = req.add_messages();
        msg->set_role("user");
        msg->set_content(prompt);
        req.set_max_tokens(512);
        req.set_temperature(0.7);

        chromelab::ChatResponse resp;
        auto s = stub->ChatComplete(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << resp.content() << "\n";
        std::cerr << "\n[" << resp.tokens_generated() << " tokens, "
                  << resp.prompt_eval_ms() << "ms prompt, "
                  << resp.eval_ms() << "ms eval]\n";
        return 0;
    }

    std::cerr << "Unknown AI action: " << action << "\n";
    return 1;
}

static const char* wg_state_str(chromelab::WGState s) {
    switch (s) {
        case chromelab::WG_STATE_UP:
            return "up";
        case chromelab::WG_STATE_DOWN:
            return "down";
        default:
            return "unknown";
    }
}

static int cmd_wg(const std::shared_ptr<grpc::Channel>& channel, const std::vector<std::string>& args, int start) {
    if (start >= static_cast<int>(args.size())) {
        std::cerr << "Usage: labctl wg <status|up|down|peers|add-peer|remove-peer|genkey> [args]\n";
        return 1;
    }

    auto stub          = chromelab::LabDaemon::NewStub(channel);
    const auto& action = args[start];

    if (action == "status") {
        grpc::ClientContext ctx;
        chromelab::WGRequest req;
        chromelab::WGStatus resp;
        auto s = stub->GetWireGuardStatus(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        std::cout << "WireGuard: " << wg_state_str(resp.state()) << "\n"
                  << "  Interface: " << resp.interface_name() << "\n"
                  << "  Listen:    " << resp.listen_port() << "\n"
                  << "  DNS:       " << resp.dns() << "\n"
                  << "  Key:       " << (resp.public_key().empty() ? "(none)" : resp.public_key().substr(0, 16) + "...") << "\n"
                  << "  Peers:     " << resp.peers_size() << "\n";

        if (!resp.error().empty()) {
            std::cerr << "  Error: " << resp.error() << "\n";
        }

        return 0;
    }

    if (action == "up") {
        grpc::ClientContext ctx;
        chromelab::WGRequest req;
        chromelab::WGStatus resp;
        auto s = stub->WireGuardUp(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (!resp.error().empty()) {
            std::cerr << "Error: " << resp.error() << "\n";
            return 1;
        }

        std::cout << "WireGuard up: " << resp.interface_name() << "\n";
        return 0;
    }

    if (action == "down") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::WGStatus resp;
        auto s = stub->WireGuardDown(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (!resp.error().empty()) {
            std::cerr << "Error: " << resp.error() << "\n";
            return 1;
        }

        std::cout << "WireGuard down\n";
        return 0;
    }

    if (action == "peers") {
        grpc::ClientContext ctx;
        chromelab::Empty req;
        chromelab::PeersList resp;
        auto s = stub->ListPeers(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (resp.peers_size() == 0) {
            std::cout << "No peers.\n";
            return 0;
        }

        std::cout << "PUBLIC KEY          ENDPOINT          ALLOWED IPS\n";
        std::cout << "──────────────────────────────────────────────────────────\n";
        for (const auto& p : resp.peers()) {
            std::string pk_short = p.public_key().substr(0, 16);
            std::cout << pk_short;
            for (int i = pk_short.size(); i < 20; ++i) {
                std::cout << ' ';
            }

            std::string ep = p.endpoint().empty() ? "-" : p.endpoint();
            std::cout << ep;
            for (int i = ep.size(); i < 18; ++i) {
                std::cout << ' ';
            }

            std::cout << (p.allowed_ips().empty() ? "-" : p.allowed_ips()) << "\n";

            if (p.latest_handshake_ms() > 0) {
                auto hs = std::chrono::milliseconds(p.latest_handshake_ms());
                auto tp = std::chrono::system_clock::time_point(hs);
                auto tt = std::chrono::system_clock::to_time_t(tp);
                char buf[20];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt));
                std::cout << "  last handshake: " << buf
                          << "  rx=" << p.transfer_rx_bytes() / 1024 << "KB"
                          << "  tx=" << p.transfer_tx_bytes() / 1024 << "KB\n";
            }
        }

        return 0;
    }

    if (action == "add-peer") {
        if (start + 3 >= static_cast<int>(args.size())) {
            std::cerr << "Usage: labctl wg add-peer <name> <public_key> <allowed_ips> [endpoint]\n";
            return 1;
        }

        grpc::ClientContext ctx;
        chromelab::AddPeerRequest req;
        req.set_name(args[start + 1]);
        req.set_public_key(args[start + 2]);
        req.set_allowed_ips(args[start + 3]);
        if (start + 4 < static_cast<int>(args.size())) {
            req.set_endpoint(args[start + 4]);
        }

        chromelab::WGStatus resp;
        auto s = stub->AddPeer(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (!resp.error().empty()) {
            std::cerr << "Error: " << resp.error() << "\n";
            return 1;
        }

        std::cout << "Peer added: " << req.name() << "\n";
        return 0;
    }

    if (action == "remove-peer") {
        if (start + 1 >= static_cast<int>(args.size())) {
            std::cerr << "Public key required\n";
            return 1;
        }

        grpc::ClientContext ctx;
        chromelab::RemovePeerRequest req;
        req.set_public_key(args[start + 1]);

        chromelab::WGStatus resp;
        auto s = stub->RemovePeer(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (!resp.error().empty()) {
            std::cerr << "Error: " << resp.error() << "\n";
            return 1;
        }

        std::cout << "Peer removed.\n";
        return 0;
    }

    if (action == "genkey") {
        auto [privkey, pubkey] = chromelab::WireGuardManager::GenerateKeypair();
        if (privkey.empty()) {
            std::cerr << "Failed to generate keypair (is wg installed?)\n";
            return 1;
        }

        std::cout << "Private key: " << privkey << "\n"
                  << "Public key:  " << pubkey << "\n"
                  << "\nKeep the private key secret. Add the public key to your peer config.\n";
        return 0;
    }

    std::cerr << "Unknown WireGuard action: " << action << "\n";
    return 1;
}

static int cmd_config(const std::shared_ptr<grpc::Channel>& channel, const std::vector<std::string>& args, int start) {
    if (start >= static_cast<int>(args.size())) {
        std::cerr << "Usage: labctl config <validate> [file]\n";
        return 1;
    }

    auto stub          = chromelab::LabDaemon::NewStub(channel);
    const auto& action = args[start];

    if (action == "validate") {
        std::string toml_str;

        // If a file path is provided, read it
        if (start + 1 < static_cast<int>(args.size())) {
            std::ifstream f(args[start + 1]);
            if (!f.is_open()) {
                std::cerr << "Cannot open: " << args[start + 1] << "\n";
                return 1;
            }

            std::ostringstream ss;
            ss << f.rdbuf();
            toml_str = ss.str();
        }

        grpc::ClientContext ctx;
        chromelab::Config req;
        req.set_raw_toml(toml_str);
        chromelab::ValidateResponse resp;
        auto s = stub->ValidateConfig(&ctx, req, &resp);
        if (!s.ok()) {
            std::cerr << "Error: " << s.error_message() << "\n";
            return 1;
        }

        if (resp.valid()) {
            std::cout << "Config: valid\n";
        } else {
            std::cout << "Config: INVALID\n";
        }

        for (const auto& e : resp.errors()) {
            std::cerr << "  error:   " << e << "\n";
        }
        for (const auto& w : resp.warnings()) {
            std::cout << "  warning: " << w << "\n";
        }

        return resp.valid() ? 0 : 1;
    }

    std::cerr << "Unknown config action: " << action << "\n";
    return 1;
}

int main(int argc, char* argv[]) {
    std::string socket_path = "/run/chromelab/labd.sock";

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--socket") == 0) && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            args.push_back(argv[i]);
        }
    }

    if (args.empty()) {
        usage(argv[0]);
        return 1;
    }

    auto channel    = connect_daemon(socket_path);
    const auto& cmd = args[0];

    if (cmd == "status") {
        return cmd_status(channel);
    } if (cmd == "info" || cmd == "system") {
        return cmd_info(channel);
    }     if (cmd == "metrics") {
        bool watch = (args.size() > 1 && args[1] == "--watch");
        return cmd_metrics(channel, watch);
    } if (cmd == "events") {
        bool tail = (args.size() > 1 && args[1] == "--tail");
        return cmd_events(channel, tail);
    } if (cmd == "svc") {
        return cmd_svc(channel, args, 1);
    } if (cmd == "wg") {
        return cmd_wg(channel, args, 1);
    } if (cmd == "ai") {
        return cmd_ai(channel, args, 1);
    } if (cmd == "config") {
        return cmd_config(channel, args, 1);
    } if (cmd == "tui") {
        chromelab::Tui tui(socket_path);
        return tui.Run();
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    usage(argv[0]);
    return 1;
}
