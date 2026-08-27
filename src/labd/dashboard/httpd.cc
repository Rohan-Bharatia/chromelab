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

#include "labd/dashboard/httpd.h"

namespace chromelab {
    static std::string ProtoToJson(const google::protobuf::Message& msg) {
        std::string json;
        google::protobuf::util::JsonPrintOptions opts;
        opts.preserve_proto_field_names = true;
        opts.always_print_enums_as_ints = false;
        (void)google::protobuf::util::MessageToJsonString(msg, &json, opts);
        return json;
    }

    HttpServer::HttpServer(uint16_t port, const std::string& web_dir, CollectorOrchestrator* collector, EventStore* events, ServiceManager* services)
        : m_port(port),
          m_web_dir(web_dir),
          m_collector(collector),
          m_events(events),
          m_services(services) {}

    HttpServer::~HttpServer(void) {
        Stop();
    }

    bool HttpServer::Start(void) {
        if (m_daemon != nullptr) {
            return true;
        }

        m_daemon = MHD_start_daemon(MHD_USE_AUTO_INTERNAL_THREAD, m_port, nullptr, nullptr, &RequestHandler, this, MHD_OPTION_END);
        return m_daemon != nullptr;
    }

    void HttpServer::Stop(void) {
        if (m_daemon != nullptr) {
            MHD_stop_daemon(m_daemon);
            m_daemon = nullptr;
        }
    }

    bool HttpServer::IsRunning(void) const {
        return m_daemon != nullptr;
    }


    void HttpServer::SetWireGuardActive(bool active) {
        m_wg_active = active;
    }

    void HttpServer::SetTailscaleActive(bool active) {
        m_ts_active = active;
    }

    MHD_Result HttpServer::RequestHandler(void* cls, struct MHD_Connection* connection, const char* url, const char* method,
                                          const char* version, const char* upload_data, size_t* upload_data_size, void** con_cls) {
        auto* self = static_cast<HttpServer*>(cls);
        return self->HandleRequest(connection, url, method);
    }

    MHD_Result HttpServer::HandleRequest(struct MHD_Connection* connection, const char* url, const char* method) {
        std::string path(url);

        if (std::strcmp(method, "GET") != 0) {
            return SendJson(connection, 405, "{\"error\":\"method not allowed\"}");
        }

        if (path == "/api/status") {
            return HandleApiStatus(connection);
        } if (path == "/api/metrics") {
            return HandleApiMetrics(connection);
        } if (path == "/api/events") {
            return HandleApiEvents(connection);
        } if (path == "/api/services") {
            return HandleApiServices(connection);
        } if (path == "/api/stream") {
            return HandleApiStream(connection);
        }

        // Static files
        if (path == "/") {
            path = "/index.html";
        }

        return HandleStatic(connection, path);
    }

    MHD_Result HttpServer::HandleApiStatus(struct MHD_Connection* connection) {
        MetricSnapshot snap = m_collector->GetSnapshot();
        auto services       = m_services->ListServices();

        int running = 0;
        for (const auto& svc : services) {
            if (svc.state() == SERVICE_STATE_RUNNING) {
                ++running;
            }
        }

        std::string json = "{";
        json            += "\"version\":\"0.1.0\"";
        json            += ",\"hostname\":\"chromelab\"";
        json            += ",\"uptime_seconds\":" + std::to_string(snap.uptime().uptime_seconds());
        json            += ",\"uptime_human\":\"" + snap.uptime().uptime_human() + "\"";
        json            += ",\"services_running\":" + std::to_string(running);
        json            += ",\"services_total\":" + std::to_string(services.size());
        json            += ",\"wireguard_active\":" + std::string(m_wg_active ? "true" : "false");
        json            += ",\"tailscale_active\":" + std::string(m_ts_active ? "true" : "false");
        json            += "}";

        return SendJson(connection, 200, json);
    }

    MHD_Result HttpServer::HandleApiMetrics(struct MHD_Connection* connection) {
        MetricSnapshot snap = m_collector->GetSnapshot();
        return SendJson(connection, 200, ProtoToJson(snap));
    }

    MHD_Result HttpServer::HandleApiEvents(struct MHD_Connection* connection) {
        int limit     = 50;
        const char* v = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "limit");
        if (v) limit  = std::max(1, std::min(500, std::atoi(v)));

        auto events = m_events->Query(limit);

        std::string json = "[";
        bool first       = true;
        for (const auto& ev : events) {
            if (!first) json += ",";
            first             = false;
            json             += ProtoToJson(ev);
        }
        json += "]";

        return SendJson(connection, 200, json);
    }

    MHD_Result HttpServer::HandleApiServices(struct MHD_Connection* connection) {
        auto services = m_services->ListServices();

        std::string json = "[";
        bool first       = true;
        for (const auto& svc : services) {
            if (!first) json += ",";
            first             = false;
            json             += ProtoToJson(svc);
        }
        json += "]";

        return SendJson(connection, 200, json);
    }

    MHD_Result HttpServer::HandleApiStream(struct MHD_Connection* connection) {
        // SSE: Send one metric snapshot, then close. Client reconnects.
        MetricSnapshot snap = m_collector->GetSnapshot();
        std::string data    = ProtoToJson(snap);

        std::string body = "data: " + data + "\n\n";

        struct MHD_Response* response = MHD_create_response_from_buffer(body.size(), const_cast<char*>(body.c_str()), MHD_RESPMEM_MUST_COPY);

        MHD_add_response_header(response, "Content-Type", "text/event-stream");
        MHD_add_response_header(response, "Cache-Control", "no-cache");
        MHD_add_response_header(response, "Connection", "keep-alive");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    MHD_Result HttpServer::HandleStatic(struct MHD_Connection* connection, const std::string& path) {
        // Prevent directory traversal
        if (path.find("..") != std::string::npos) {
            return SendNotFound(connection);
        }

        std::string full = m_web_dir + path;
        std::ifstream file(full, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return SendNotFound(connection);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string body(size, '\0');
        file.read(body.data(), size);

        std::string mime       = MimeFor(path);
        if (mime.empty()) mime = "application/octet-stream";

        struct MHD_Response* response = MHD_create_response_from_buffer(body.size(), const_cast<char*>(body.c_str()), MHD_RESPMEM_MUST_COPY);

        MHD_add_response_header(response, "Content-Type", mime.c_str());
        MHD_add_response_header(response, "Cache-Control", "no-cache");

        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    MHD_Result HttpServer::SendJson(struct MHD_Connection* connection, int status, const std::string& json) {
        struct MHD_Response* response = MHD_create_response_from_buffer(json.size(), const_cast<char*>(json.c_str()), MHD_RESPMEM_MUST_COPY);

        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

        MHD_Result ret = MHD_queue_response(connection, status, response);
        MHD_destroy_response(response);
        return ret;
    }

    MHD_Result HttpServer::SendNotFound(struct MHD_Connection* connection) {
        std::string body              = "{\"error\":\"not found\"}";
        struct MHD_Response* response = MHD_create_response_from_buffer(body.size(), const_cast<char*>(body.c_str()), MHD_RESPMEM_MUST_COPY);

        MHD_add_response_header(response, "Content-Type", "application/json");

        MHD_Result ret = MHD_queue_response(connection, 404, response);
        MHD_destroy_response(response);
        return ret;
    }

    std::string HttpServer::MimeFor(const std::string& ext) const {
        auto pos = ext.rfind('.');
        if (pos == std::string::npos) {
            return "";
        }
        std::string e = ext.substr(pos);

        if (e == ".html" || e == ".htm") {
            return "text/html; charset=utf-8";
        } if (e == ".css") {
            return "text/css; charset=utf-8";
        } if (e == ".js") {
            return "application/javascript; charset=utf-8";
        } if (e == ".json") {
            return "application/json";
        } if (e == ".png") {
            return "image/png";
        } if (e == ".jpg" || e == ".jpeg") {
            return "image/jpeg";
        } if (e == ".gif") {
            return "image/gif";
        } if (e == ".svg") {
            return "image/svg+xml";
        } if (e == ".ico") {
            return "image/x-icon";
        } if (e == ".woff") {
            return "font/woff";
        } if (e == ".woff2") {
            return "font/woff2";
        } if (e == ".ttf") {
            return "font/ttf";
        }
        return "application/octet-stream";
    }
} // namespace chromelab
