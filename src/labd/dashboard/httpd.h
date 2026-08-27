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

#ifndef _LABD_DASHBOARD_HTTD_H_
    #define _LABD_DASHBOARD_HTTD_H_ (1)

#include "labd/collector/collector.h"
#include "labd/events/store.h"
#include "labd/services/manager.h"

namespace chromelab {
    class HttpServer {
    public:
        HttpServer(uint16_t port, const std::string& web_dir, CollectorOrchestrator* collector, EventStore* events, ServiceManager* services);
        ~HttpServer(void);

        HttpServer(const HttpServer&)            = delete;
        HttpServer& operator=(const HttpServer&) = delete;

        bool Start(void);
        void Stop(void);
        bool IsRunning(void) const;
        void SetWireGuardActive(bool active);
        void SetTailscaleActive(bool active);

    private:
        static MHD_Result RequestHandler(void* cls, struct MHD_Connection* connection, const char* url, const char* method,
                                         const char* version, const char* upload_data, size_t* upload_data_size, void** con_cls);
        MHD_Result HandleRequest(struct MHD_Connection* connection, const char* url, const char* method);
        MHD_Result HandleApiStatus(struct MHD_Connection* connection);
        MHD_Result HandleApiMetrics(struct MHD_Connection* connection);
        MHD_Result HandleApiEvents(struct MHD_Connection* connection);
        MHD_Result HandleApiServices(struct MHD_Connection* connection);
        MHD_Result HandleApiStream(struct MHD_Connection* connection);
        MHD_Result HandleStatic(struct MHD_Connection* connection, const std::string& path);
        MHD_Result SendJson(struct MHD_Connection* connection, int status, const std::string& json);
        MHD_Result SendNotFound(struct MHD_Connection* connection);
        std::string MimeFor(const std::string& ext) const;

        uint16_t m_port;
        std::string m_web_dir;
        CollectorOrchestrator* m_collector;
        EventStore* m_events;
        ServiceManager* m_services;
        struct MHD_Daemon* m_daemon = nullptr;
        bool m_wg_active            = false;
        bool m_ts_active            = false;
    };
} // namespace chromelab

#endif // _LABD_DASHBOARD_HTTD_H_
