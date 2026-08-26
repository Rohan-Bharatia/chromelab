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

#include "labd/collector/cpu.h"

namespace chromelab {
    CpuCollector::CpuCollector(void) = default;

    void CpuCollector::Collect(MetricSnapshot& snap) {
        std::ifstream stat("/proc/stat");
        if (!stat.is_open()) {
            return;
        }

        CpuMetrics* cpu = snap.mutable_cpu();
        std::vector<CpuTicks> current_ticks;

        std::string line;
        while (std::getline(stat, line)) {
            if (line.compare(0, 3, "cpu") != 0) {
                break;
            }

            std::istringstream iss(line);
            std::string label;
            CpuTicks t{};
            iss >> label >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;

            // Check if this is the aggregate "cpu" line or a per-core "cpuN" line
            if (label == "cpu") {
                cpu->set_overall_percent(0.0); // computed below
            } else {
                current_ticks.push_back(t);
            }
        }

        // Compute per-core utilization from deltas
        if (m_first_sample) {
            m_prev_ticks   = current_ticks;
            m_first_sample = false;
            return;
        }

        double total_percent = 0.0;
        int core_count       = std::min(current_ticks.size(), m_prev_ticks.size());

        for (int i = 0; i < core_count; ++i) {
            auto prev = m_prev_ticks[i];
            auto curr = current_ticks[i];

            int64_t prev_total   = prev.Total();
            int64_t curr_total   = curr.Total();
            int64_t total_delta  = curr_total - prev_total;
            int64_t active_delta = curr.Active() - prev.Active();

            double percent = (total_delta > 0) ? (static_cast<double>(active_delta) / static_cast<double>(total_delta)) * 100.0 : 0.0;

            auto* core = cpu->add_cores();
            core->set_core_id(i);
            core->set_percent(percent);
            core->set_user(curr.user);
            core->set_system(curr.system);
            core->set_idle(curr.idle);

            total_percent += percent;
        }

        if (core_count > 0) {
            cpu->set_overall_percent(total_percent / core_count);
        }

        m_prev_ticks = current_ticks;
    }
} // namespace chromelab
