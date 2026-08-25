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
        m_config(config) {}

    void LabDaemonImpl::Run() {
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

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        Stop();
    }

    void LabDaemonImpl::Stop() {
        if (m_server) {
            std::cout << "labd shutting down...\n";
            m_server->Shutdown();
            m_server.reset();
        }

        m_running = false;
    }

    grpc::Status LabDaemonImpl::GetStatus(grpc::ServerContext* ctx, const Empty* req, StatusResponse* resp) {
        resp->set_version("0.1.0");
        resp->set_hostname("chromelab");
        resp->set_uptime_seconds(0);
        resp->set_ai_loaded(false);
        resp->set_wireguard_active(false);
        resp->set_services_running(0);
        resp->set_services_total(0);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetSystemInfo(grpc::ServerContext* ctx, const Empty* req, SystemInfo* resp) {
        resp->set_hostname("chromelab");
        resp->set_kernel_version("unknown");
        resp->set_alpine_version("unknown");
        resp->set_arch("x86_64");
        resp->set_total_ram_bytes(0);
        resp->set_total_disk_bytes(0);
        resp->set_cpu_cores(0);
        resp->set_cpu_model("unknown");

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
        resp->set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamMetrics(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<MetricSnapshot>* writer) {
        int interval = req->interval_ms() > 0 ? req->interval_ms() : m_config.metrics_interval_ms;

        while (g_running.load()) {
            MetricSnapshot snap;
            snap.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            if (!writer->Write(snap)) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListEvents(grpc::ServerContext* ctx, const EventsRequest* req, EventsResponse* resp) {
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamEvents(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<Event>* writer) {
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::EmitEvent(grpc::ServerContext* ctx, const Event* req, EmitResponse* resp) {
        resp->set_event_id(0);

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
