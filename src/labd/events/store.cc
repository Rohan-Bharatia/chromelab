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

#include "labd/events/store.h"

namespace chromelab {
    EventStore::EventStore(size_t max_events)
        : m_max(max_events) {}

    EventStore::~EventStore(void) {
        if (m_disk_file.is_open()) {
            m_disk_file.close();
        }
    }

    int64_t EventStore::Append(Event* event) {
        ++m_sequence;

        if (event->id() == 0) {
            event->set_id(m_sequence);
        }

        if (event->timestamp_ms() == 0) {
            event->set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }

        if (m_disk_enabled) {
            AppendToDisk(*event);
        }

        std::unique_lock lock(m_mutex);
        if (m_ring.size() >= m_max) {
            m_ring.pop_front();
        }
        m_ring.push_back(*event);

        return event->id();
    }

    std::vector<Event> EventStore::Query(int limit, EventCategory category, Severity severity, int64_t since_ms) const {
        std::shared_lock lock(m_mutex);
        std::vector<Event> results;

        // Walk backwards (newest first)
        for (auto it = m_ring.rbegin(); it != m_ring.rend(); ++it) {
            if (results.size() >= static_cast<size_t>(limit)) {
                break;
            }

            if (category != EVENT_CATEGORY_UNSPECIFIED && it->category() != category) {
                continue;
            } if (severity != SEVERITY_UNSPECIFIED && it->severity() != severity) {
                continue;
            } if (since_ms > 0 && it->timestamp_ms() < since_ms) {
                continue;
            }

            results.push_back(*it);
        }

        // Reverse so newest is last (matches proto repeated convention)
        std::reverse(results.begin(), results.end());
        return results;
    }

    bool EventStore::EnableDisk(const std::string& directory) {
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) {
            std::cerr << "EventStore: failed to create " << directory << ": " << ec.message() << "\n";
            return false;
        }

        auto now  = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::tm tm_buf{};
        localtime_r(&time, &tm_buf);

        char filename[64];
        std::strftime(filename, sizeof(filename), "events-%Y%m%d.jsonl", &tm_buf);

        std::string path = directory + "/" + filename;

        std::unique_lock lock(m_mutex);
        m_disk_file.open(path, std::ios::app);
        if (!m_disk_file.is_open()) {
            std::cerr << "EventStore: failed to open " << path << "\n";
            return false;
        }

        m_disk_enabled = true;
        m_disk_dir     = directory;
        return true;
    }

    int64_t EventStore::Sequence(void) const {
        return m_sequence;
    }

    void EventStore::AppendToDisk(const Event& event) {
        // Append one JSON line: {"id":...,"ts":...,"cat":...,"sev":...,"src":"...","msg":"..."}
        std::string line = "{\"id\":";
        line            += std::to_string(event.id());
        line            += ",\"ts\":";
        line            += std::to_string(event.timestamp_ms());
        line            += ",\"cat\":";
        line            += std::to_string(static_cast<int>(event.category()));
        line            += ",\"sev\":";
        line            += std::to_string(static_cast<int>(event.severity()));
        line            += ",\"src\":\"";
        line            += event.source();
        line            += "\",\"msg\":\"";

        // Escape any double quotes or backslashes in the message
        for (char c : event.message()) {
            if (c == '"') {
                line += "\\\"";
            } else if (c == '\\') {
                line += "\\\\";
            } else if (c == '\n') {
                line += "\\n";
            } else {
                line += c;
            }
        }
        line += "\"}";

        std::lock_guard lock(m_mutex);
        if (m_disk_file.is_open()) {
            m_disk_file << line << "\n";
            m_disk_file.flush();
        }
    }
} // namespace chromelab
