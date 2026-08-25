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

#include "labd/server.h"

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "  -s, --socket PATH   Unix socket path (default: /run/chromelab/labd.sock)\n"
              << "  -w, --web-dir PATH  Web dashboard directory\n"
              << "  -i, --interval MS   Metrics collection interval in ms (default: 2000)\n"
              << "  -l, --log-level L   Log level: debug, info, warn, error (default: info)\n"
              << "  -h, --help          Show this help\n";
}

int main(int argc, char* argv[]) {
    chromelab::LabdConfig config;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if ((std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--socket") == 0) && i + 1 < argc) {
            config.socket_path = argv[++i];
        } else if ((std::strcmp(argv[i], "-w") == 0 || std::strcmp(argv[i], "--web-dir") == 0) && i + 1 < argc) {
            config.web_dir = argv[++i];
        } else if ((std::strcmp(argv[i], "-i") == 0 || std::strcmp(argv[i], "--interval") == 0) && i + 1 < argc) {
            config.metrics_interval_ms = std::stoi(argv[++i]);
        } else if ((std::strcmp(argv[i], "-l") == 0 || std::strcmp(argv[i], "--log-level") == 0) && i + 1 < argc) {
            config.log_level = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    signal(SIGINT,  [](int) {});
    signal(SIGTERM, [](int) {});

    // Ensure socket directory exists
    auto socket_dir = std::filesystem::path(config.socket_path).parent_path();
    if (!socket_dir.empty()) {
        std::filesystem::create_directories(socket_dir);
    }

    // Remove stale socket
    if (std::filesystem::exists(config.socket_path)) {
        std::filesystem::remove(config.socket_path);
    }

    std::cout << "labd v0.1.0 starting...\n"
              << "  socket: " << config.socket_path << "\n"
              << "  metrics interval: " << config.metrics_interval_ms << "ms\n";

    chromelab::LabDaemonImpl daemon(config);
    daemon.Run();

    return 0;
}
