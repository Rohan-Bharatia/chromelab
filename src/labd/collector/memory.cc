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

#include "labd/collector/memory.h"

namespace chromelab {
    void MemoryCollector::Collect(MetricSnapshot& snap) {
        std::ifstream meminfo("/proc/meminfo");
        if (!meminfo.is_open()) {
            return;
        }

        MemoryMetrics* mem = snap.mutable_memory();
        std::string line;
        int64_t mem_total  = 0,
                mem_free   = 0,
                buffers    = 0,
                cached     = 0;
        int64_t swap_total = 0,
                swap_free  = 0;

        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            int64_t value;
            std::string unit;
            iss >> key >> value >> unit;

            // Strip trailing ':'
            if (!key.empty() && key.back() == ':') {
                key.pop_back();
            }

            if (key == "MemTotal") {
                mem_total = value;
            } else if (key == "MemFree") {
                mem_free = value;
            } else if (key == "Buffers") {
                buffers = value;
            } else if (key == "Cached") {
                cached = value;
            } else if (key == "SwapTotal") {
                swap_total = value;
            } else if (key == "SwapFree") {
                swap_free = value;
            }
        }

        int64_t used = mem_total - mem_free - buffers - cached;
        if (used < 0) {
            used = 0;
        }

        // Values are in kB, convert to bytes
        mem->set_total_bytes(mem_total * 1024);
        mem->set_free_bytes(mem_free * 1024);
        mem->set_buffers_bytes(buffers * 1024);
        mem->set_cached_bytes(cached * 1024);
        mem->set_swap_total_bytes(swap_total * 1024);
        mem->set_swap_used_bytes((swap_total - swap_free) * 1024);
        mem->set_used_bytes(used * 1024);
        mem->set_percent(mem_total > 0 ? (static_cast<double>(used) / mem_total) * 100.0 : 0.0);
        mem->set_swap_percent(swap_total > 0 ? (static_cast<double>(swap_total - swap_free) / swap_total) * 100.0 : 0.0);
    }
} // namespace chromelab
