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

#include "labd/collector/disk.h"

namespace chromelab {
    DiskCollector::DiskCollector(const std::vector<std::string>& device_filter)
        : m_filter(device_filter) {}

    void DiskCollector::Collect(MetricSnapshot& snap) {
        DiskMetrics* disk = snap.mutable_disk();

        // Parse /proc/diskstats for I/O counters
        std::ifstream diskstats("/proc/diskstats");
        if (diskstats.is_open()) {
            std::string line;
            while (std::getline(diskstats, line)) {
                std::istringstream iss(line);
                int major, minor;
                std::string name;
                int64_t reads_completed, reads_merged, sectors_read, ms_reading;
                int64_t writes_completed, writes_merged, sectors_written, ms_writing;
                int64_t ios_in_progress, ms_io, weighted_ms_io;

                iss >> major >> minor >> name
                    >> reads_completed >> reads_merged >> sectors_read >> ms_reading
                    >> writes_completed >> writes_merged >> sectors_written >> ms_writing
                    >> ios_in_progress >> ms_io >> weighted_ms_io;

                // Only include whole-disk devices (skip partitions like sda1, mmcblk0p1)
                bool is_partition = false;
                if (!name.empty()) {
                    char last = name.back();
                    if (last >= '0' && last <= '9') {
                        // Check if second-to-last is a letter (e.g., sda1) or 'p' (e.g., mmcblk0p1)
                        if (name.size() >= 2) {
                            char prev = name[name.size() - 2];
                            if (prev == 'p' || (prev >= 'a' && prev <= 'z')) {
                                // Could be partition, check if there's a non-digit base
                                size_t digit_start = name.size() - 1;
                                while (digit_start > 0 && name[digit_start - 1] >= '0' && name[digit_start - 1] <= '9') {
                                    --digit_start;
                                }

                                // If there are digits before the last set of digits, it's likely a partition
                                if (digit_start > 0 && name[digit_start - 1] == 'p') {
                                    is_partition = true;
                                }
                            }
                        }
                    }
                }

                if (is_partition) {
                    continue;
                }

                // Apply device filter
                if (!m_filter.empty()) {
                    bool found = false;
                    for (const auto& f : m_filter) {
                        if (name == f) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        continue;
                    }
                }

                auto* dev = disk->add_disks();
                dev->set_name(name);
                dev->set_reads_completed(reads_completed);
                dev->set_reads_bytes(sectors_read * 512);
                dev->set_writes_completed(writes_completed);
                dev->set_writes_bytes(sectors_written * 512);
                dev->set_io_in_progress(ios_in_progress);
                dev->set_io_time_ms(ms_io);
            }
        }

        // Check common mount points
        static const char* mount_points[] = {"/", "/home", "/tmp", "/var", "/boot"};
        for (const auto& mp : mount_points) {
            struct statvfs vfs {};
            if (statvfs(mp, &vfs) != 0) {
                continue;
            } if (vfs.f_blocks == 0) {
                continue;
            }

            uint64_t total     = vfs.f_blocks * vfs.f_frsize;
            uint64_t available = vfs.f_bavail * vfs.f_frsize;
            uint64_t used      = total - (vfs.f_bfree * vfs.f_frsize);

            auto* fs = disk->add_filesystems();
            fs->set_mount_point(mp);
            fs->set_total_bytes(total);
            fs->set_used_bytes(used);
            fs->set_available_bytes(available);
            fs->set_percent(total > 0 ? (static_cast<double>(used) / total) * 100.0 : 0.0);
        }
    }
} // namespace chromelab
