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

#include "labd/services/manager.h"

namespace chromelab {
    ServiceManager::ServiceManager(EventBus* bus)
        : m_bus(bus) {}

    std::pair<int, std::string> ServiceManager::Run(const std::string& cmd) {
        std::string combined;
        std::array<char, 256> buf{};

        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
        if (!pipe) {
            return {-1, "popen failed"};
        }

        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
            combined += buf.data();
        }

        int rc        = pclose(pipe);
        int exit_code = WEXITSTATUS(rc);

        // Trim trailing whitespace
        while (!combined.empty() && (combined.back() == '\n' || combined.back() == '\r' || combined.back() == ' ')) {
            combined.pop_back();
        }

        return { exit_code, combined };
    }

    ServiceState ServiceManager::ParseStatus(const std::string& raw) {
        if (raw.find("running") != std::string::npos) {
            return SERVICE_STATE_RUNNING;
        } if (raw.find("stopped") != std::string::npos) {
            return SERVICE_STATE_STOPPED;
        } if (raw.find("starting") != std::string::npos) {
            return SERVICE_STATE_STARTING;
        } if (raw.find("stopping") != std::string::npos) {
            return SERVICE_STATE_STOPPING;
        } if (raw.find("failed") != std::string::npos) {
            return SERVICE_STATE_FAILED;
        }
        return SERVICE_STATE_UNSPECIFIED;
    }

    bool ServiceManager::IsEnabled(const std::string& name) {
        auto [rc, output] = Run("rc-update show -v");
        if (rc != 0) {
            return false;
        }

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            // Lines look like "  service_name | default" or "   service_name: default"
            auto pos = line.find(name);
            if (pos != std::string::npos) {
                // Make sure we match the whole name, not a substring
                size_t start = (pos > 0 && line[pos - 1] == ' ') ? pos - 1 : pos;
                size_t end   = pos + name.size();
                if (end < line.size() && line[end] == ':') {
                    ++end;
                }

                return true;
            }
        }

        return false;
    }

    std::vector<ServiceInfo> ServiceManager::ListServices(void) const {
        std::vector<ServiceInfo> result;

        auto [rc, output] = Run("rc-service --list");
        if (rc != 0) {
            return result;
        }

        std::istringstream stream(output);
        std::string name;
        while (std::getline(stream, name)) {
            // Trim whitespace
            while (!name.empty() && name.front() == ' ') {
                name.erase(name.begin());
            }
            while (!name.empty() && (name.back() == '\n' || name.back() == '\r' || name.back() == ' ')) {
                name.pop_back();
            }

            if (name.empty()) {
                continue;
            }

            // Get status for this service
            auto [src, status_out] = Run("rc-service " + name + " status");
            ServiceState state     = ParseStatus(status_out);
            bool enabled           = IsEnabled(name);

            ServiceInfo info;
            info.set_name(name);
            info.set_state(state);
            info.set_enabled(enabled);
            info.set_runlevel("default");
            result.push_back(std::move(info));
        }

        return result;
    }

    std::optional<ServiceInfo> ServiceManager::GetService(const std::string& name) const {
        // Check if it exists by trying status
        auto [rc, output] = Run("rc-service " + name + " status");
        if (rc != 0 && output.find("not found") != std::string::npos) {
            return std::nullopt;
        }

        ServiceInfo info;
        info.set_name(name);
        info.set_state(ParseStatus(output));
        info.set_enabled(IsEnabled(name));
        info.set_runlevel("default");
        return info;
    }

    ServiceManager::ActionResult ServiceManager::Start(const std::string& name) {
        ActionResult result;
        result.name = name;

        // Get previous state
        auto prev = GetService(name);
        if (prev) {
            result.previous_state = prev->state();
        }

        auto [rc, output] = Run("rc-service " + name + " start");
        if (rc != 0) {
            result.error = output.empty() ? "start failed (exit " + std::to_string(rc) + ")" : output;
            return result;
        }

        auto current         = GetService(name);
        result.current_state = current ? current->state() : SERVICE_STATE_UNSPECIFIED;

        if (m_bus) {
            Event event;
            event.set_category(EVENT_CATEGORY_SERVICE);
            event.set_severity(SEVERITY_INFO);
            event.set_source("service-manager");
            event.set_message("Service started: " + name);
            (*m_bus).Emit(event);
        }

        return result;
    }

    ServiceManager::ActionResult ServiceManager::Stop(const std::string& name) {
        ActionResult result;
        result.name = name;

        auto prev = GetService(name);
        if (prev) {
            result.previous_state = prev->state();
        }

        auto [rc, output] = Run("rc-service " + name + " stop");
        if (rc != 0) {
            result.error = output.empty() ? "stop failed (exit " + std::to_string(rc) + ")" : output;
            return result;
        }

        auto current         = GetService(name);
        result.current_state = current ? current->state() : SERVICE_STATE_UNSPECIFIED;

        if (m_bus) {
            Event event;
            event.set_category(EVENT_CATEGORY_SERVICE);
            event.set_severity(SEVERITY_INFO);
            event.set_source("service-manager");
            event.set_message("Service stopped: " + name);
            (*m_bus).Emit(event);
        }

        return result;
    }

    ServiceManager::ActionResult ServiceManager::Restart(const std::string& name) {
        ActionResult result;
        result.name = name;

        auto prev = GetService(name);
        if (prev) result.previous_state = prev->state();

        auto [rc, output] = Run("rc-service " + name + " restart");
        if (rc != 0) {
            result.error = output.empty() ? "restart failed (exit " + std::to_string(rc) + ")" : output;
            return result;
        }

        auto current         = GetService(name);
        result.current_state = current ? current->state() : SERVICE_STATE_UNSPECIFIED;

        if (m_bus) {
            Event event;
            event.set_category(EVENT_CATEGORY_SERVICE);
            event.set_severity(SEVERITY_INFO);
            event.set_source("service-manager");
            event.set_message("Service restarted: " + name);
            (*m_bus).Emit(event);
        }

        return result;
    }

    ServiceManager::ActionResult ServiceManager::Enable(const std::string& name) {
        ActionResult result;
        result.name = name;

        auto prev = GetService(name);
        if (prev) result.previous_state = prev->state();

        auto [rc, output] = Run("rc-update add " + name + " default");
        if (rc != 0) {
            result.error = output.empty() ? "enable failed (exit " + std::to_string(rc) + ")" : output;
            return result;
        }

        auto current         = GetService(name);
        result.current_state = current ? current->state() : SERVICE_STATE_UNSPECIFIED;

        if (m_bus) {
            Event event;
            event.set_category(EVENT_CATEGORY_SERVICE);
            event.set_severity(SEVERITY_INFO);
            event.set_source("service-manager");
            event.set_message("Service enabled: " + name);
            (*m_bus).Emit(event);
        }

        return result;
    }

    ServiceManager::ActionResult ServiceManager::Disable(const std::string& name) {
        ActionResult result;
        result.name = name;

        auto prev = GetService(name);
        if (prev) result.previous_state = prev->state();

        auto [rc, output] = Run("rc-update del " + name + " default");
        if (rc != 0) {
            result.error = output.empty() ? "disable failed (exit " + std::to_string(rc) + ")" : output;
            return result;
        }

        auto current         = GetService(name);
        result.current_state = current ? current->state() : SERVICE_STATE_UNSPECIFIED;

        if (m_bus) {
            Event event;
            event.set_category(EVENT_CATEGORY_SERVICE);
            event.set_severity(SEVERITY_INFO);
            event.set_source("service-manager");
            event.set_message("Service disabled: " + name);
            (*m_bus).Emit(event);
        }

        return result;
    }

    HealthStatus ServiceManager::CheckHealth(const std::string& name) const {
        HealthStatus status;
        status.set_service_name(name);

        auto [rc, output] = Run("rc-service " + name + " status");
        bool running      = (rc == 0);

        status.set_healthy(running);
        status.set_message(output);
        status.set_checked_at_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        return status;
    }
} // namespace chromelab
