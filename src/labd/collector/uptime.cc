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

#include "labd/collector/uptime.h"

namespace chromelab {
    void UptimeCollector::Collect(MetricSnapshot& snap) {
        std::ifstream f("/proc/uptime");
        if (!f.is_open()) {
            return;
        }

        SystemUptime* up  = snap.mutable_uptime();
        double uptime_sec = 0.0;
        f >> uptime_sec;

        int64_t total_sec = static_cast<int64_t>(uptime_sec);
        up->set_uptime_seconds(total_sec);

        // Human-readable format
        int days  = total_sec / 86400;
        int hours = (total_sec % 86400) / 3600;
        int mins  = (total_sec % 3600) / 60;
        int secs  = total_sec % 60;

        std::string human;
        if (days > 0) {
            human += std::to_string(days) + "d ";
        } if (hours > 0) {
            human += std::to_string(hours) + "h ";
        } if (mins > 0) {
            human += std::to_string(mins) + "m ";
        }
        human += std::to_string(secs) + "s";

        up->set_uptime_human(human);
    }
} // namespace chromelab
