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

#ifndef _LABD_EVENTS_BUS_H_
    #define _LABD_EVENTS_BUS_H_ (1)

#include "pch.h"

namespace chromelab {
    // A simple pub/sub event bus
    class EventBus {
    public:
        using Callback = std::function<void(const Event&)>;

        class Subscription {
        public:
            Subscription(void) noexcept = default;
            ~Subscription(void);

            Subscription(Subscription&& other) noexcept;
            Subscription& operator=(Subscription&& other) noexcept;

            Subscription(const Subscription&) = delete;
            Subscription& operator=(const Subscription&) = delete;

            void Cancel(void);

        private:
            friend class EventBus;
            explicit Subscription(uint64_t id, EventBus* bus);

            uint64_t m_id   = 0;
            EventBus* m_bus = nullptr;
        };

        EventBus(void) = default;
        ~EventBus(void) = default;

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        Subscription Subscribe(EventCategory category, Callback cb);

        // Emit an event to all matching subscribers (called on the emitter's thread)
        void Emit(const Event& event);

    private:
        struct Entry {
            uint64_t id;
            EventCategory category;
            Callback callback;
        };

        std::mutex m_mutex;
        std::vector<Entry> m_entries;
        uint64_t m_next_id = 1;
    };
} // namespace chromelab

#endif // _LABD_EVENTS_BUS_H_
