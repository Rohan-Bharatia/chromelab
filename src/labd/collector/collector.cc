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

#include "labd/collector/collector.h"
#include "labd/collector/cpu.h"
#include "labd/collector/memory.h"
#include "labd/collector/disk.h"
#include "labd/collector/network.h"
#include "labd/collector/temperature.h"
#include "labd/collector/load.h"
#include "labd/collector/processes.h"
#include "labd/collector/uptime.h"

namespace chromelab {
    CollectorOrchestrator::CollectorOrchestrator(const LabdConfig& config)
        : m_interval_ms(config.metrics_interval_ms) {

        // Register collectors based on config
        if (config.tel_cpu) {
            m_collectors.push_back(std::make_unique<CpuCollector>());
        } if (config.tel_memory) {
            m_collectors.push_back(std::make_unique<MemoryCollector>());
        } if (config.tel_disk) {
            m_collectors.push_back(std::make_unique<DiskCollector>(config.tel_disk_devices));
        } if (config.tel_network) {
            m_collectors.push_back(std::make_unique<NetworkCollector>(config.tel_network_interfaces));
        } if (config.tel_temperature) {
            m_collectors.push_back(std::make_unique<TemperatureCollector>());
        } if (config.tel_load) {
            m_collectors.push_back(std::make_unique<LoadCollector>());
        } if (config.tel_processes) {
            m_collectors.push_back(std::make_unique<ProcessCollector>());
        } if (config.tel_uptime) {
            m_collectors.push_back(std::make_unique<UptimeCollector>());
        }

        // Do an initial collection to populate the snapshot
        CollectAll();
    }

    CollectorOrchestrator::~CollectorOrchestrator(void) {
        Stop();
    }

    void CollectorOrchestrator::Start(void) {
        if (m_running.exchange(true)) {
            return; // already running
        }

        m_thread = std::thread([this]() {
            while (m_running.load()) {
                CollectAll();
                std::this_thread::sleep_for(std::chrono::milliseconds(m_interval_ms));
            }
        });
    }

    void CollectorOrchestrator::Stop(void) {
        m_running.store(false);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    MetricSnapshot CollectorOrchestrator::GetSnapshot() const {
        std::lock_guard lock(m_mutex);
        return m_latest;
    }

    MetricSnapshot CollectorOrchestrator::WaitForSnapshot() {
        return GetSnapshot();
    }

    void CollectorOrchestrator::CollectAll(void) {
        MetricSnapshot snap;
        snap.set_timestamp_ms(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        for (auto& collector : m_collectors) {
            collector->Collect(snap);
        }

        std::lock_guard lock(m_mutex);
        m_latest = std::move(snap);
    }
} // namespace chromelab
