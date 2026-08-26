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

#ifndef _LABD_EVENTS_STORE_H_
    #define _LABD_EVENTS_STORE_H_ (1)

namespace chromelab {
    // Thread-safe ring buffer for protobuf Event objects with optional append-to-disk support
    class EventStore {
    public:
        explicit EventStore(size_t max_events = 10000);
        ~EventStore(void);

        EventStore(const EventStore&)            = delete;
        EventStore& operator=(const EventStore&) = delete;

        int64_t Append(Event* event);

        std::vector<Event> Query(int limit = 100, EventCategory category = EVENT_CATEGORY_UNSPECIFIED,
                                 Severity severity = SEVERITY_UNSPECIFIED, int64_t since_ms = 0) const;

        bool EnableDisk(const std::string& directory);

        int64_t Sequence(void) const;

    private:
        void AppendToDisk(const Event& event);

        mutable std::shared_mutex m_mutex;
        std::deque<Event> m_ring;
        size_t m_max;
        int64_t m_sequence = 0;

        // Disk persistence
        bool m_disk_enabled = false;
        std::string m_disk_dir;
        std::ofstream m_disk_file;
    };
} // namespace chromelab

#endif // _LABD_EVENTS_STORE_H_
