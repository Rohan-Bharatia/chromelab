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

#include "labd/server.h"

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    g_running.store(false);
}

namespace chromelab {
    LabDaemonImpl::LabDaemonImpl(const LabdConfig& config) :
        m_config(config),
        m_collector(std::make_unique<CollectorOrchestrator>(config, &m_bus)),
        m_store(10000) {

        if (!config.events_dir.empty()) {
            m_store.EnableDisk(config.events_dir);
        }
    }

    void LabDaemonImpl::Run(void) {
        m_running = true;

        grpc::ServerBuilder builder;
        builder.AddListeningPort("unix:" + m_config.socket_path, grpc::InsecureServerCredentials());
        builder.RegisterService(this);

        m_server = builder.BuildAndStart();
        if (!m_server) {
            std::cerr << "Failed to start gRPC server on " << m_config.socket_path << "\n";
            return;
        }

        std::cout << "labd listening on " << m_config.socket_path << "\n";

        // Emit startup event
        {
            Event startup;
            startup.set_category(EVENT_CATEGORY_SYSTEM);
            startup.set_severity(SEVERITY_INFO);
            startup.set_source("labd");
            startup.set_message("Daemon started");
            m_store.Append(&startup);
            m_bus.Emit(startup);
        }

        // Start telemetry collection
        m_collector->Start();

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        Stop();
    }

    void LabDaemonImpl::Stop(void) {
        // Stop telemetry collection first
        if (m_collector) {
            m_collector->Stop();
        }

        if (m_server) {
            std::cout << "labd shutting down...\n";
            m_server->Shutdown();
            m_server.reset();
        }

        m_running = false;
    }

    grpc::Status LabDaemonImpl::GetStatus(grpc::ServerContext* ctx, const Empty* req, StatusResponse* resp) {
        auto snap = m_collector->GetSnapshot();
        resp->set_version("0.1.0");
        resp->set_hostname("chromelab");
        resp->set_uptime_seconds(snap.uptime().uptime_seconds());
        resp->set_ai_loaded(false);
        resp->set_wireguard_active(false);
        resp->set_services_running(0);
        resp->set_services_total(0);

        return grpc::Status::OK;
    }

    static std::string read_first_line(const std::string& path) {
        std::ifstream f(path);
        std::string line;
        if (f.is_open()) std::getline(f, line);
        return line;
    }

    grpc::Status LabDaemonImpl::GetSystemInfo(grpc::ServerContext* ctx, const Empty* req, SystemInfo* resp) {
        resp->set_hostname("chromelab");
        resp->set_kernel_version(read_first_line("/proc/version"));

        // Read CPU info
        std::ifstream cpuinfo("/proc/cpuinfo");
        int core_count = 0;
        std::string cpu_model;
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.compare(0, 8, "model name") == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) cpu_model = line.substr(pos + 2);
            }
            if (line.compare(0, 9, "processor") == 0) core_count++;
        }
        resp->set_cpu_model(cpu_model);
        resp->set_cpu_cores(core_count);
        resp->set_arch("x86_64");

        // Read memory from latest snapshot
        auto snap = m_collector->GetSnapshot();
        resp->set_total_ram_bytes(snap.memory().total_bytes());
        resp->set_total_disk_bytes(0);

        // Disk total from filesystems
        int64_t total_disk = 0;
        for (const auto& fs : snap.disk().filesystems()) {
            total_disk += fs.total_bytes();
        }
        resp->set_total_disk_bytes(total_disk);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ValidateConfig(grpc::ServerContext* ctx, const Config* req, ValidateResponse* resp) {
        resp->set_valid(true);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::Reboot(grpc::ServerContext* ctx, const RebootRequest* req, Empty* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::Shutdown(grpc::ServerContext* ctx, const Empty* req, Empty* resp) {
        g_running.store(false);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetMetrics(grpc::ServerContext* ctx, const MetricsRequest* req, MetricSnapshot* resp) {
        *resp = m_collector->GetSnapshot();
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamMetrics(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<MetricSnapshot>* writer) {
        int interval = req->interval_ms() > 0 ? req->interval_ms() : m_config.metrics_interval_ms;

        while (g_running.load()) {
            MetricSnapshot snap = m_collector->GetSnapshot();
            if (!writer->Write(snap)) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListEvents(grpc::ServerContext* ctx, const EventsRequest* req, EventsResponse* resp) {
        auto events = m_store.Query(req->limit() > 0 ? req->limit() : 100, req->category_filter(), req->severity_filter(), req->since_ms());
        for (auto& ev : events) {
            *resp->add_events() = std::move(ev);
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamEvents(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<Event>* writer) {
        // Subscribe to all events and forward them to the gRPC writer
        auto sub = m_bus.Subscribe(EVENT_CATEGORY_UNSPECIFIED, [&](const Event& event) {
            Event copy = event;
            writer->Write(copy);
        });

        // Block until client disconnects
        while (g_running.load() && !ctx->IsCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::EmitEvent(grpc::ServerContext* ctx, const Event* req, EmitResponse* resp) {
        Event event = *req;
        int64_t id  = m_store.Append(&event);
        m_bus.Emit(event);

        resp->set_event_id(id);
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListServices(grpc::ServerContext* ctx, const Empty* req, ServicesList* resp) {
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceInfo* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::StartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::StopService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::RestartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::EnableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::DisableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::CheckServiceHealth(grpc::ServerContext* ctx, const ServiceRequest* req, HealthStatus* resp) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not yet");
    }

    grpc::Status LabDaemonImpl::GetAIStatus(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) {
        resp->set_status(ModelStatus::MODEL_STATUS_UNLOADED);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListModels(grpc::ServerContext* ctx, const Empty* req, ModelsList* resp) {
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::LoadModel(grpc::ServerContext* ctx, const ModelRequest* req, AIStatus* resp) {
        resp->set_status(ModelStatus::MODEL_STATUS_ERROR);
        resp->set_error("AI module not yet implemented");

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::UnloadModel(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) {
        resp->set_status(ModelStatus::MODEL_STATUS_UNLOADED);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ChatComplete(grpc::ServerContext* ctx, const ChatRequest* req, ChatResponse* resp) {
        resp->set_content("AI module not yet implemented");

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamChat(grpc::ServerContext* ctx, const ChatRequest* req, grpc::ServerWriter<ChatToken>* writer) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "AI not yet implemented");
    }

    grpc::Status LabDaemonImpl::GetWireGuardStatus(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) {
        resp->set_state(WGState::WG_STATE_DOWN);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::WireGuardUp(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) {
        resp->set_state(WGState::WG_STATE_DOWN);
        resp->set_error("WireGuard module not yet implemented");

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::WireGuardDown(grpc::ServerContext* ctx, const Empty* req, WGStatus* resp) {
        resp->set_state(WGState::WG_STATE_DOWN);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::AddPeer(grpc::ServerContext* ctx, const AddPeerRequest* req, WGStatus* resp) {
        resp->set_error("WireGuard module not yet implemented");

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::RemovePeer(grpc::ServerContext* ctx, const RemovePeerRequest* req, WGStatus* resp) {
        resp->set_error("WireGuard module not yet implemented");

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListPeers(grpc::ServerContext* ctx, const Empty* req, PeersList* resp) {
        return grpc::Status::OK;
    }
} // namespace chromelab
