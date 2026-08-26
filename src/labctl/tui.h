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

#ifndef _LABCTL_TUI_H_
    #define _LABCTL_TUI_H_ (1)

#include "pch.h"

namespace chromelab {
    class Tui {
    public:
        explicit Tui(const std::string& socket_path);
        ~Tui(void);

        Tui(const Tui&)            = delete;
        Tui& operator=(const Tui&) = delete;

        int Run(void);

    private:
        void InitNcurses(void);
        void DrawHeader(void);
        void DrawCpu(const MetricSnapshot& snap);
        void DrawMemory(const MetricSnapshot& snap);
        void DrawLoadDisk(const MetricSnapshot& snap);
        void DrawNetwork(const MetricSnapshot& snap);
        void DrawEvents(void);
        void DrawServices(void);
        void DrawStatusBar(void);
        void DrawHelp(void);
        void RefreshData(void);

        std::string m_socket;
        std::shared_ptr<grpc::Channel> m_channel;
        std::unique_ptr<LabDaemon::Stub> m_stub;

        // Cached data
        MetricSnapshot m_snap;
        std::vector<Event> m_events;
        std::vector<ServiceInfo> m_services;

        // Layout
        int m_rows     = 0,
            m_cols     = 0;
        bool m_running = true;
    };
} // namespace chromelab

#endif // _LABCTL_TUI_H_
