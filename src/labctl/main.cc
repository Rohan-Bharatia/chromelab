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

#include "pch.h"

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
              << "  svc start <name>    Start a service\n"
              << "  svc stop <name>     Stop a service\n"
              << "  svc restart <name>  Restart a service\n"
              << "  svc enable <name>   Enable a service\n"
              << "  svc disable <name>  Disable a service\n"
              << "  ai status           AI module status\n"
              << "  ai models           List available models\n"
              << "  ai load <model>     Load a model\n"
              << "  ai unload           Unload current model\n"
              << "  ai chat <prompt>    Chat with loaded model\n"
              << "  wg status           WireGuard status\n"
              << "  wg up               Bring WireGuard up\n"
              << "  wg down             Bring WireGuard down\n"
              << "  config validate     Validate configuration\n"
              << "  system info         Show system info\n"
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

static int cmd_metrics(const std::shared_ptr<grpc::Channel>& channel, bool watch) {
    auto stub = chromelab::LabDaemon::NewStub(channel);

    if (watch) {
        grpc::ClientContext ctx;
        chromelab::StreamRequest req;
        req.set_interval_ms(2000);
        auto reader = stub->StreamMetrics(&ctx, req);

        chromelab::MetricSnapshot snap;
        while (reader->Read(&snap)) {
            std::cout << "\r[" << snap.timestamp_ms() << "] metrics received" << std::flush;
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

    std::cout << "Metrics snapshot at " << resp.timestamp_ms() << "\n";
    return 0;
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

        for (const auto& svc : resp.services()) {
            std::cout << svc.name() << "\t"
                      << (svc.enabled() ? "enabled" : "disabled") << "\t"
                      << svc.description() << "\n";
        }

        return 0;
    }

    if (start + 1 >= static_cast<int>(args.size())) {
        std::cerr << "Service name required\n";
        return 1;
    }

    chromelab::ServiceRequest req;
    req.set_name(args[start + 1]);
    chromelab::ServiceResponse resp;
    grpc::ClientContext ctx;

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
    } else {
        std::cerr << "Unknown action: " << action << "\n";
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

    std::cout << resp.name() << ": " << resp.current_state() << "\n";
    return 0;
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
    } if (cmd == "metrics") {
        bool watch = (args.size() > 1 && args[1] == "--watch");
        return cmd_metrics(channel, watch);
    } if (cmd == "svc") {
        return cmd_svc(channel, args, 1);
    } if (cmd == "wg") {
        std::cout << "WireGuard commands not yet implemented\n";
        return 1;
    } if (cmd == "ai") {
        std::cout << "AI commands not yet implemented\n";
        return 1;
    } if (cmd == "config") {
        std::cout << "Config commands not yet implemented\n";
        return 1;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    usage(argv[0]);
    return 1;
}
