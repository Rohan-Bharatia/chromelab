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

#include "labd/server.h"

static constexpr const char* LABD_VERSION = "0.1.0";

static chromelab::LabDaemonImpl* g_daemon = nullptr;
static std::atomic<bool>         g_reload_requested{false};
static std::atomic<bool>         g_shutdown_requested{false};

static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            g_shutdown_requested.store(true);
            break;
        case SIGHUP:
            g_reload_requested.store(true);
            break;
    }
}

static bool daemonize_process(pid_t& out_pid) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork failed: " << strerror(errno) << "\n";
        return false;
    }
    if (pid > 0) {
        out_pid = pid;
        return true; // parent
    }

    if (setsid() < 0) {
        _exit(1);
    }

    // Suppress warn_unused_result warnings
    (void)!freopen("/dev/null", "r", stdin);
    (void)!freopen("/dev/null", "w", stdout);
    (void)!freopen("/dev/null", "w", stderr);

    return false; // child continues
}

static void write_pid_file(const std::string& path, pid_t pid) {
    std::error_code ec;
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream ofs(path);
    ofs << pid << "\n";
}

static void remove_pid_file(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

static void usage(const char* prog) {
    std::cerr
        << "labd v" << LABD_VERSION << " - chromelab daemon\n\n"
        << "Usage: " << prog << " [options]\n\n"
        << "Options:\n"
        << "  -c, --config PATH     Config file (default: /etc/chromelab/labd.conf)\n"
        << "  -s, --socket PATH     Override Unix socket path\n"
        << "  -p, --port PORT       Override HTTP dashboard port\n"
        << "  -i, --interval MS     Override metrics interval (ms)\n"
        << "  -l, --log-level LEVEL Override log level (debug/info/warn/error)\n"
        << "  -d, --daemon          Fork to background (daemonize)\n"
        << "      --no-daemon       Force foreground mode\n"
        << "  -v, --version         Print version and exit\n"
        << "  -h, --help            Show this help\n\n"
        << "Signals:\n"
        << "  SIGINT/SIGTERM        Graceful shutdown\n"
        << "  SIGHUP                Reload configuration\n\n"
        << "Config is loaded from --config path. CLI flags override config values.\n";
}

static bool reload_config(const std::string& path, chromelab::LabdConfig& cfg) {
    std::string err;
    chromelab::LabdConfig fresh;
    if (!chromelab::LoadConfig(path, fresh, err)) {
        std::cerr << "config reload failed: " << err << "\n";
        return false;
    }

    cfg = fresh;
    std::cout << "config reloaded from " << path << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    std::string config_path = "/etc/chromelab/labd.conf";
    chromelab::LabdConfig config; // starts with compiled defaults

    bool cli_socket       = false;
    bool cli_port         = false;
    bool cli_interval     = false;
    bool cli_loglevel     = false;
    bool force_daemon     = false;
    bool force_foreground = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                _exit(1);
            }

            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "labd " << LABD_VERSION << "\n";
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            config_path = next();
        } else if (arg == "-s" || arg == "--socket") {
            config.socket_path = next();
            cli_socket = true;
        } else if (arg == "-p" || arg == "--port") {
            config.http_port = static_cast<uint16_t>(std::stoi(next()));
            cli_port         = true;
        } else if (arg == "-i" || arg == "--interval") {
            config.metrics_interval_ms = std::stoi(next());
            cli_interval               = true;
        } else if (arg == "-l" || arg == "--log-level") {
            config.log_level = next();
            cli_loglevel     = true;
        } else if (arg == "-d" || arg == "--daemon") {
            force_daemon = true;
        } else if (arg == "--no-daemon") {
            force_foreground = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    if (std::filesystem::exists(config_path)) {
        std::string err;
        if (!chromelab::LoadConfig(config_path, config, err)) {
            std::cerr << "FATAL: " << err << "\n";
            return 1;
        }
        std::cout << "loaded config from " << config_path << "\n";
    } else if (config_path == "/etc/chromelab/labd.conf") {
        std::cout << "no config file found, using defaults\n";
    } else {
        std::cerr << "FATAL: config file not found: " << config_path << "\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto skip_next = [&]() { ++i; };

        if (arg == "-c" || arg == "--config" || arg == "-h" || arg == "--help" || arg == "-v" || arg == "--version" || arg == "-d" || arg == "--daemon" || arg == "--no-daemon") {
            if (arg != "-c" && arg != "--config") {
                skip_next();
            }

            continue;
        } if (arg == "-s" || arg == "--socket") {
            config.socket_path = argv[++i];
        } else if (arg == "-p" || arg == "--port") {
            config.http_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-i" || arg == "--interval") {
            config.metrics_interval_ms = std::stoi(argv[++i]);
        } else if (arg == "-l" || arg == "--log-level") {
            config.log_level = argv[++i];
        }
    }

    // Resolve daemonize mode
    if (force_foreground) {
        config.daemonize = false;
    } else if (force_daemon) {
        config.daemonize = true;
    }

    if (config.daemonize) {
        pid_t parent_pid = 0;
        bool is_child    = !daemonize_process(parent_pid);
        if (!is_child) {
            write_pid_file(config.pid_file, parent_pid);
            std::cout << "labd daemonized, pid=" << parent_pid << "\n";
            return 0;
        }
    }

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP,  &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    auto ensure_dir = [](const std::string& path) {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
        }
    };

    ensure_dir(std::filesystem::path(config.socket_path).parent_path());
    ensure_dir(config.events_dir);
    ensure_dir(config.models_dir);
    if (!config.log_file.empty()) {
        ensure_dir(std::filesystem::path(config.log_file).parent_path());
    }

    std::error_code ec;
    if (std::filesystem::exists(config.socket_path, ec)) {
        std::filesystem::remove(config.socket_path, ec);
    }

    // Write PID file
    write_pid_file(config.pid_file, getpid());

    std::cout << "labd v" << LABD_VERSION << "\n"
              << "  socket:   " << config.socket_path << "\n"
              << "  http:     " << (config.http_port > 0 ? std::to_string(config.http_port) : "disabled") << "\n"
              << "  metrics:  " << config.metrics_interval_ms << "ms\n"
              << "  log:      " << config.log_level << "\n"
              << "  pid:      " << getpid() << "\n";

    chromelab::LabDaemonImpl daemon(config);
    g_daemon = &daemon;

    std::thread daemon_thread([&daemon]() {
        daemon.Run();
    });

    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (g_reload_requested.exchange(false)) {
            reload_config(config_path, config);
        }
    }

    std::cout << "shutting down...\n";
    daemon.Stop();
    daemon_thread.join();
    remove_pid_file(config.pid_file);
    std::cout << "labd stopped.\n";

    return 0;
}
