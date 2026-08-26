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

#include "labd/events/bus.h"

namespace chromelab {
    EventBus::Subscription::Subscription(uint64_t id, EventBus* bus)
        : m_id(id), m_bus(bus) {}

    EventBus::Subscription::~Subscription(void) {
        Cancel();
    }

    EventBus::Subscription::Subscription(Subscription&& other) noexcept
        : m_id(other.m_id),
        m_bus(other.m_bus) {
        other.m_id  = 0;
        other.m_bus = nullptr;
    }

    EventBus::Subscription& EventBus::Subscription::operator=(Subscription&& other) noexcept {
        if (this != &other) {
            Cancel();
            m_id        = other.m_id;
            m_bus       = other.m_bus;
            other.m_id  = 0;
            other.m_bus = nullptr;
        }

        return *this;
    }

    void EventBus::Subscription::Cancel(void) {
        if (m_bus != nullptr && m_id != 0) {
            std::lock_guard lock(m_bus->m_mutex);
            auto& entries = m_bus->m_entries;
            entries.erase(std::remove_if(entries.begin(), entries.end(), [this](const Entry& e) { return e.id == m_id; }), entries.end());
        }

        m_id  = 0;
        m_bus = nullptr;
    }

    EventBus::Subscription EventBus::Subscribe(EventCategory category, Callback cb) {
        uint64_t id;
        {
            std::lock_guard lock(m_mutex);
            id = m_next_id++;
            m_entries.push_back({id, category, std::move(cb)});
        }

        return Subscription(id, this);
    }

    void EventBus::Emit(const Event& event) {
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_entries) {
            if (entry.category == EVENT_CATEGORY_UNSPECIFIED || entry.category == event.category()) {
                entry.callback(event);
            }
        }
    }
} // namespace chromelab
