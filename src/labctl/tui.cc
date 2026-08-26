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

namespace chromelab {
    Tui::Tui(const std::string& socket_path)
        : m_socket(socket_path),
          m_channel(grpc::CreateChannel("unix:" + socket_path, grpc::InsecureChannelCredentials())),
          m_stub(LabDaemon::NewStub(m_channel)) {}

    Tui::~Tui(void) {
        if (m_running) {
            endwin();
        }
    }

    void Tui::InitNcurses(void) {
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        timeout(0);
        start_color();
        init_pair(1, COLOR_GREEN,  COLOR_BLACK); // running
        init_pair(2, COLOR_RED,    COLOR_BLACK); // failed/critical
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // warning
        init_pair(4, COLOR_CYAN,   COLOR_BLACK); // header
        init_pair(5, COLOR_BLUE,   COLOR_BLACK); // dim
    }

    void Tui::RefreshData(void) {
        // Metrics
        grpc::ClientContext ctx;
        MetricsRequest mreq;
        m_stub->GetMetrics(&ctx, mreq, &m_snap);

        // Events
        grpc::ClientContext ectx;
        EventsRequest ereq;
        ereq.set_limit(30);
        EventsResponse eresp;
        m_stub->ListEvents(&ectx, ereq, &eresp);
        m_events.clear();
        for (auto& e : *eresp.mutable_events()) {
            m_events.push_back(std::move(e));
        }

        // Services
        grpc::ClientContext sctx;
        Empty sreq;
        ServicesList sresp;
        m_stub->ListServices(&sctx, sreq, &sresp);
        m_services.clear();
        for (auto& s : *sresp.mutable_services()) {
            m_services.push_back(std::move(s));
        }
    }

    static void DrawHLine(int row, int col, int width, chtype ch = ACS_HLINE) {
        move(row, col);
        for (int i = 0; i < width; i++) addch(ch);
    }

    static void DrawBar(int row, int col, int width, double pct, int pair = 1) {
        int filled = static_cast<int>(pct / 100.0 * width);
        if (filled > width) filled = width;
        attron(COLOR_PAIR(pair));
        move(row, col);
        for (int i = 0; i < width; i++) {
            addch(i < filled ? '#' : '.');
        }
        attroff(COLOR_PAIR(pair));
    }

    static std::string FmtUptime(int64_t seconds) {
        int d = seconds / 86400;
        int h = (seconds % 86400) / 3600;
        int m = (seconds % 3600) / 60;
        if (d > 0) {
            return std::to_string(d) + "d " + std::to_string(h) + "h";
        } if (h > 0) {
            return std::to_string(h) + "h " + std::to_string(m) + "m";
        }

        return std::to_string(m) + "m";
    }

    static std::string FmtBytes(int64_t b) {
        if (b > 1073741824) {
            return std::to_string(b / 1073741824) + " GB";
        } if (b > 1048576) {
            return std::to_string(b / 1048576) + " MB";
        } if (b > 1024) {
            return std::to_string(b / 1024) + " KB";
        }
        return std::to_string(b) + " B";
    }

    static const char* SevStr(Severity s) {
        switch (s) {
            case SEVERITY_INFO:
                return "INFO";
            case SEVERITY_WARNING:
                return "WARN";
            case SEVERITY_ERROR:
                return "ERR ";
            case SEVERITY_CRITICAL:
                return "CRIT";
            default:
                return "----";
        }
    }

    static const char* StateStr(ServiceState s) {
        switch (s) {
            case SERVICE_STATE_RUNNING:
                return "running";
            case SERVICE_STATE_STOPPED:
                return "stopped";
            case SERVICE_STATE_STARTING:
                return "starting";
            case SERVICE_STATE_STOPPING:
                return "stopping";
            case SERVICE_STATE_FAILED:
                return "failed";
            default:
                return "unknown";
        }
    }

    void Tui::DrawHeader(void) {
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(0, 1, "chromelab");
        attroff(COLOR_PAIR(4) | A_BOLD);
        mvprintw(0, 12, " uptime: %s", FmtUptime(m_snap.uptime().uptime_seconds()).c_str());
        mvprintw(0, m_cols - 25, "q:quit  r:refresh");
    }

    void Tui::DrawCpu(const MetricSnapshot& snap) {
        int row = 2;
        attron(A_BOLD);
        mvprintw(row, 1, "CPU  %.0f%%", snap.cpu().overall_percent());
        attroff(A_BOLD);
        DrawBar(row + 1, 1, 30, snap.cpu().overall_percent(), 1);

        row += 3;
        for (int i = 0; i < snap.cpu().cores_size() && i < 4; ++i) {
            const auto& c = snap.cpu().cores(i);
            mvprintw(row, 1, "c%d %.0f%%", c.core_id(), c.percent());
            DrawBar(row, 10, 20, c.percent(), c.percent() > 80 ? 3 : 1);
            ++row;
        }
    }

    void Tui::DrawMemory(const MetricSnapshot& snap) {
        int row       = 2;
        const int col = m_cols / 3;

        attron(A_BOLD);
        mvprintw(row, col, "MEM  %.0f%%", snap.memory().percent());
        attroff(A_BOLD);
        DrawBar(row + 1, col, 30, snap.memory().percent(), 1);

        row += 3;
        mvprintw(row, col, "%s / %s", FmtBytes(snap.memory().used_bytes()).c_str(), FmtBytes(snap.memory().total_bytes()).c_str());
        if (snap.memory().swap_total_bytes() > 0) {
            mvprintw(row + 1, col, "swap %s / %s", FmtBytes(snap.memory().swap_used_bytes()).c_str(), FmtBytes(snap.memory().swap_total_bytes()).c_str());
        }
    }

    void Tui::DrawLoadDisk(const MetricSnapshot& snap) {
        int row       = 2;
        const int col = 2 * m_cols / 3;

        attron(A_BOLD);
        mvprintw(row, col, "LOAD %.2f %.2f %.2f", snap.load().load_1m(), snap.load().load_5m(), snap.load().load_15m());
        attroff(A_BOLD);

        row += 2;
        for (int i = 0; i < snap.disk().filesystems_size(); ++i) {
            const auto& fs = snap.disk().filesystems(i);
            mvprintw(row, col, "%s %.0f%%", fs.mount_point().c_str(), fs.percent());
            DrawBar(row + 1, col, 25, fs.percent(), fs.percent() > 90 ? 2 : 1);
            row          += 3;
        }
    }

    void Tui::DrawNetwork(const MetricSnapshot& snap) {
        int row       = 8;
        const int col = 2 * m_cols / 3;

        attron(A_BOLD);
        mvprintw(row, col, "NET");
        attroff(A_BOLD);
        ++row;

        for (int i = 0; i < snap.network().interfaces_size() && i < 3; ++i) {
            const auto& iface = snap.network().interfaces(i);
            mvprintw(row, col, "%s rx:%s tx:%s", iface.name().c_str(), FmtBytes(iface.rx_bytes()).c_str(), FmtBytes(iface.tx_bytes()).c_str());
            ++row;
        }

        mvprintw(row, col, "tcp:%d udp:%d", snap.network().tcp_established(), snap.network().udp_connections());
    }

    void Tui::DrawEvents(void) {
        int row = 14;
        attron(A_BOLD);
        mvprintw(row, 1, "EVENTS");
        attroff(A_BOLD);
        ++row;

        int max   = m_rows - row - 3;
        int start = std::max(0, static_cast<int>(m_events.size()) - max);
        int col_w = m_cols / 3 - 2;

        for (int i = start; i < static_cast<int>(m_events.size()); ++i) {
            const auto& ev = m_events[i];
            auto ts        = std::chrono::system_clock::time_point(std::chrono::milliseconds(ev.timestamp_ms()));
            auto tt        = std::chrono::system_clock::to_time_t(ts);
            char buf[10];
            std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&tt));

            int sev_pair = 1;
            if (ev.severity() == SEVERITY_ERROR || ev.severity() == SEVERITY_CRITICAL) {
                sev_pair = 2;
            } else if (ev.severity() == SEVERITY_WARNING) {
                sev_pair = 3;
            }

            attron(COLOR_PAIR(sev_pair));
            mvprintw(row, 1, "%s %s", buf, SevStr(ev.severity()));
            attroff(COLOR_PAIR(sev_pair));

            std::string msg = ev.source() + ": " + ev.message();
            if (static_cast<int>(msg.size()) > col_w - 16) {
                msg = msg.substr(0, col_w - 19) + "...";
            }
            mvprintw(row, 16, "%s", msg.c_str());
            ++row;
        }
    }

    void Tui::DrawServices(void) {
        int row       = 14;
        const int col = m_cols / 3 + 1;
        attron(A_BOLD);
        mvprintw(row, col, "SERVICES");
        attroff(A_BOLD);
        ++row;

        int max = m_rows - row - 3;
        int n   = std::min(max, static_cast<int>(m_services.size()));

        for (int i = 0; i < n; ++i) {
            const auto& svc = m_services[i];
            const char* st  = StateStr(svc.state());

            int pair = 1;
            if (svc.state() == SERVICE_STATE_RUNNING) {
                pair = 1;
            } else if (svc.state() == SERVICE_STATE_FAILED) {
                pair = 2;
            } else {
                pair = 5;
            }

            attron(COLOR_PAIR(pair));
            mvprintw(row, col, "%-16s %s", svc.name().c_str(), st);
            attroff(COLOR_PAIR(pair));

            if (svc.enabled()) {
                mvprintw(row, col + 26, "*");
            }
            ++row;
        }
    }

    void Tui::DrawStatusBar(void) {
        int row     = m_rows - 1;
        DrawHLine(row - 1, 0, m_cols, ACS_HLINE);
        mvprintw(row, 1, "services: ");
        int running = 0;
        for (const auto& s : m_services) {
            if (s.state() == SERVICE_STATE_RUNNING) running++;
        }
        printw("%d/%d running", running, static_cast<int>(m_services.size()));
        mvprintw(row, m_cols - 40, "procs: %d total, %d zombie", m_snap.processes().total(), m_snap.processes().zombie());
    }

    void Tui::DrawHelp(void) {
        // Not shown permanently
    }

    int Tui::Run(void) {
        InitNcurses();

        auto last_refresh = std::chrono::steady_clock::now();
        auto last_input   = last_refresh;

        while (m_running) {
            auto now = std::chrono::steady_clock::now();

            // Refresh data every 2 seconds
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refresh).count() >= 2000) {
                RefreshData();
                last_refresh = now;
            }

            // Draw
            getmaxyx(stdscr, m_rows, m_cols);
            erase();

            DrawHeader();
            DrawCpu(m_snap);
            DrawMemory(m_snap);
            DrawLoadDisk(m_snap);
            DrawNetwork(m_snap);
            DrawEvents();
            DrawServices();
            DrawStatusBar();

            refresh();

            // Handle input (non-blocking, 200ms timeout)
            timeout(200);
            int ch = getch();
            if (ch == 'q' || ch == 'Q') {
                m_running = false;
            } else if (ch == 'r' || ch == 'R') {
                RefreshData();
                last_refresh = std::chrono::steady_clock::now();
            }
        }

        endwin();
        return 0;
    }
} // namespace chromelab
