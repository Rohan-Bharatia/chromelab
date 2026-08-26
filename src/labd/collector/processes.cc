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

#include "labd/collector/processes.h"

namespace chromelab {
    void ProcessCollector::Collect(MetricSnapshot& snap) {
        ProcessMetrics* procs = snap.mutable_processes();
        int32_t total         = 0,
                running       = 0,
                sleeping      = 0,
                stopped       = 0,
                zombie        = 0;
        namespace fs          = std::filesystem;

        for (const auto& entry : fs::directory_iterator("/proc")) {
            auto& path = entry.path();
            auto name  = path.filename().string();

            // Skip non-PID directories
            bool is_pid = true;
            for (char c : name) {
                if (c < '0' || c > '9') {
                    is_pid = false;
                    break;
                }
            }

            if (!is_pid) {
                continue;
            }

            ++total;

            // Read status field from /proc/<pid>/status
            std::ifstream f(path / "status");
            if (!f.is_open()) {
                continue;
            }

            std::string line;
            while (std::getline(f, line)) {
                if (line.compare(0, 6, "State:") != 0) {
                    continue;
                }

                // Format: "State:	S (sleeping)"
                if (line.size() > 7) {
                    char state = line[6];
                    switch (state) {
                        case 'R':
                            ++running;
                            break;
                        case 'S':
                        case 'I':
                            ++sleeping;
                            break;
                        case 'T':
                        case 't':
                            ++stopped;
                            break;
                        case 'Z':
                            ++zombie;
                            break;
                        default:
                            ++sleeping;
                            break;
                    }
                }
                break;
            }
        }

        procs->set_total(total);
        procs->set_running(running);
        procs->set_sleeping(sleeping);
        procs->set_stopped(stopped);
        procs->set_zombie(zombie);
    }
} // namespace chromelab
