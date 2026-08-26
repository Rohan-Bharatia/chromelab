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

#ifndef _LABD_SERVICES_MANAGER_H_
    #define _LABD_SERVICES_MANAGER_H_ (1)

#include "labd/events/bus.h"

namespace chromelab {
    // Manages system services via OpenRC
    class ServiceManager {
    public:
        explicit ServiceManager(EventBus* bus = nullptr);
        ~ServiceManager(void) = default;

        ServiceManager(const ServiceManager&) = delete;
        ServiceManager& operator=(const ServiceManager&) = delete;

        std::vector<ServiceInfo> ListServices(void) const;

        std::optional<ServiceInfo> GetService(const std::string& name) const;

        struct ActionResult {
            std::string name;
            ServiceState previous_state = SERVICE_STATE_UNSPECIFIED;
            ServiceState current_state  = SERVICE_STATE_UNSPECIFIED;
            std::string error;
        };

        ActionResult Start(const std::string& name);
        ActionResult Stop(const std::string& name);
        ActionResult Restart(const std::string& name);

        ActionResult Enable(const std::string& name);
        ActionResult Disable(const std::string& name);

        HealthStatus CheckHealth(const std::string& name) const;

    private:
        static std::pair<int, std::string> Run(const std::string& cmd);
        static ServiceState ParseStatus(const std::string& raw);
        static bool IsEnabled(const std::string& name);

        EventBus* m_bus = nullptr;
    };
} // namespace chromelab

#endif // _LABD_SERVICES_MANAGER_H_
