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

#include "labd/collector/temperature.h"

namespace chromelab {
    void TemperatureCollector::Collect(MetricSnapshot& snap) {
        TemperatureMetrics* temp = snap.mutable_temperature();
        namespace fs             = std::filesystem;

        const fs::path thermal_dir("/sys/class/thermal");
        if (!fs::exists(thermal_dir)) {
            return;
        }

        for (const auto& entry : fs::directory_iterator(thermal_dir)) {
            const auto& path = entry.path();
            auto name        = path.filename().string();

            // Only process thermal_zone* entries
            if (name.compare(0, 11, "thermal_zone") != 0) {
                continue;
            }

            // Read temperature (in millidegrees Celsius)
            int64_t temp_mc = 0;
            {
                std::ifstream f(path / "temp");
                if (!f.is_open()) {
                    continue;
                }

                f >> temp_mc;
            }

            // Read type
            std::string type;
            {
                std::ifstream f(path / "type");
                if (f.is_open()) {
                    std::getline(f, type);
                }
            }

            auto* zone = temp->add_zones();
            zone->set_name(name);
            zone->set_temp_celsius(static_cast<double>(temp_mc) / 1000.0);
            zone->set_type(type);
        }
    }
} // namespace chromelab
