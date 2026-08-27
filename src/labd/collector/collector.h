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

#ifndef _LABD_COLLECTOR_H_
    #define _LABD_COLLECTOR_H_ (1)

#include "labd/config/config.h"

namespace chromelab {
    class EventBus;

    // Base interface for all metric collectors
    class Collector {
    public:
        virtual ~Collector(void) = default;

        virtual void Collect(MetricSnapshot& snap) = 0;
    };

    // Owns all collectors, runs them on a timer thread, and provides the latest snapshot to gRPC handlers
    class CollectorOrchestrator {
    public:
        explicit CollectorOrchestrator(const LabdConfig& config, EventBus* bus = nullptr);
        ~CollectorOrchestrator(void);

        // Start the periodic collection thread.
        void Start(void);

        // Stop the collection thread and wait for it to finish.
        void Stop(void);

        // Get the latest snapshot (thread-safe).
        MetricSnapshot GetSnapshot() const;

        // Get the latest snapshot for streaming (blocks until next collection).
        MetricSnapshot WaitForSnapshot();

    private:
        void CollectAll(void);
        void CheckThresholds(const MetricSnapshot& snap);

        std::vector<std::unique_ptr<Collector>> m_collectors;
        mutable std::mutex m_mutex;
        MetricSnapshot m_latest;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
        int m_interval_ms;

        // Threshold alarms
        EventBus* m_bus = nullptr;
        bool m_was_cpu_high     = false;
        bool m_was_mem_high     = false;
        bool m_was_disk_high    = false;
        bool m_was_temp_high    = false;
        bool m_was_zombies      = false;
    };
} // namespace chromelab

#endif // _LABD_COLLECTOR_H_
