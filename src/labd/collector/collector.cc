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
#include "labd/events/bus.h"

namespace chromelab {
    CollectorOrchestrator::CollectorOrchestrator(const LabdConfig& config, EventBus* bus)
        : m_interval_ms(config.metrics_interval_ms), m_bus(bus) {

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

        {
            std::lock_guard lock(m_mutex);
            m_latest = snap;
        }

        if (m_bus != nullptr) {
            CheckThresholds(snap);
        }
    }

    static void emit_alarm(EventBus* bus, const std::string& msg, Severity sev) {
        Event event;
        event.set_category(EVENT_CATEGORY_SYSTEM);
        event.set_severity(sev);
        event.set_source("collector");
        event.set_message(msg);
        bus->Emit(event);
    }

    void CollectorOrchestrator::CheckThresholds(const MetricSnapshot& snap) {
        // CPU
        bool cpu_high = snap.cpu().overall_percent() > 90.0;
        if (cpu_high && !m_was_cpu_high) {
            std::ostringstream oss;
            oss << "CPU usage high: " << snap.cpu().overall_percent() << "%";
            emit_alarm(m_bus, oss.str(), SEVERITY_WARNING);
        }
        m_was_cpu_high = cpu_high;

        // Memory
        bool mem_high = snap.memory().percent() > 90.0;
        if (mem_high && !m_was_mem_high) {
            std::ostringstream oss;
            oss << "Memory usage high: " << snap.memory().percent() << "%";
            emit_alarm(m_bus, oss.str(), SEVERITY_WARNING);
        }
        m_was_mem_high = mem_high;

        // Disk (check any filesystem)
        bool disk_high = false;
        for (const auto& fs : snap.disk().filesystems()) {
            if (fs.percent() > 95.0) {
                disk_high = true;
                if (!m_was_disk_high) {
                    std::ostringstream oss;
                    oss << "Disk usage critical on " << fs.mount_point() << ": " << fs.percent() << "%";
                    emit_alarm(m_bus, oss.str(), SEVERITY_ERROR);
                }
                break;
            }
        }
        m_was_disk_high = disk_high;

        // Temperature
        bool temp_high = false;
        for (const auto& z : snap.temperature().zones()) {
            if (z.temp_celsius() > 80.0) {
                temp_high = true;
                if (!m_was_temp_high) {
                    std::ostringstream oss;
                    oss << "Temperature high on " << z.name() << ": " << z.temp_celsius() << " C";
                    emit_alarm(m_bus, oss.str(), SEVERITY_WARNING);
                }
                break;
            }
        }
        m_was_temp_high = temp_high;

        // Zombie processes
        bool zombies = snap.processes().zombie() > 0;
        if (zombies && !m_was_zombies) {
            std::ostringstream oss;
            oss << "Zombie processes detected: " << snap.processes().zombie();
            emit_alarm(m_bus, oss.str(), SEVERITY_ERROR);
        }
        m_was_zombies = zombies;
    }
} // namespace chromelab
