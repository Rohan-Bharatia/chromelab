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

#ifndef _LABD_SERVER_H_
    #define _LABD_SERVER_H_ (1)

#include "pch.h"

#include "labd/config.h"
#include "labd/collector/collector.h"
#include "labd/events/bus.h"
#include "labd/events/store.h"

namespace chromelab {

    class LabDaemonImpl final :
        public LabDaemon::Service {
    public:
        explicit LabDaemonImpl(const LabdConfig& config);
        ~LabDaemonImpl(void) override = default;

        void Run(void);
        void Stop(void);

        // System
        grpc::Status GetStatus(grpc::ServerContext* ctx, const Empty* req, StatusResponse* resp) override;
        grpc::Status GetSystemInfo(grpc::ServerContext* ctx, const Empty* req, SystemInfo* resp) override;
        grpc::Status ValidateConfig(grpc::ServerContext* ctx, const Config* req, ValidateResponse* resp) override;
        grpc::Status Reboot(grpc::ServerContext* ctx, const RebootRequest* req, Empty* resp) override;
        grpc::Status Shutdown(grpc::ServerContext* ctx, const Empty* req, Empty* resp) override;

        // Telemetry
        grpc::Status GetMetrics(grpc::ServerContext* ctx, const MetricsRequest* req, MetricSnapshot* resp) override;
        grpc::Status StreamMetrics(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<MetricSnapshot>* writer) override;

        // Events
        grpc::Status ListEvents(grpc::ServerContext* ctx, const EventsRequest* req, EventsResponse* resp) override;
        grpc::Status StreamEvents(grpc::ServerContext* ctx, const StreamRequest* req, grpc::ServerWriter<Event>* writer) override;
        grpc::Status EmitEvent(grpc::ServerContext* ctx, const Event* req, EmitResponse* resp) override;

        // Services
        grpc::Status ListServices(grpc::ServerContext* ctx, const Empty* req, ServicesList* resp) override;
        grpc::Status GetService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceInfo* resp) override;
        grpc::Status StartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) override;
        grpc::Status StopService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) override;
        grpc::Status RestartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) override;
        grpc::Status EnableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) override;
        grpc::Status DisableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) override;
        grpc::Status CheckServiceHealth(grpc::ServerContext* ctx, const ServiceRequest* req, HealthStatus* resp) override;

        // AI
        grpc::Status GetAIStatus(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) override;
        grpc::Status ListModels(grpc::ServerContext* ctx, const Empty* req, ModelsList* resp) override;
        grpc::Status LoadModel(grpc::ServerContext* ctx, const ModelRequest* req, AIStatus* resp) override;
        grpc::Status UnloadModel(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) override;
        grpc::Status ChatComplete(grpc::ServerContext* ctx, const ChatRequest* req, ChatResponse* resp) override;
        grpc::Status StreamChat(grpc::ServerContext* ctx, const ChatRequest* req, grpc::ServerWriter<ChatToken>* writer) override;

        // Remote Access
        grpc::Status GetWireGuardStatus(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) override;
        grpc::Status WireGuardUp(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) override;
        grpc::Status WireGuardDown(grpc::ServerContext* ctx, const Empty* req, WGStatus* resp) override;
        grpc::Status AddPeer(grpc::ServerContext* ctx, const AddPeerRequest* req, WGStatus* resp) override;
        grpc::Status RemovePeer(grpc::ServerContext* ctx, const RemovePeerRequest* req, WGStatus* resp) override;
        grpc::Status ListPeers(grpc::ServerContext* ctx, const Empty* req, PeersList* resp) override;

    private:
        LabdConfig m_config;
        std::unique_ptr<grpc::Server> m_server;
        std::unique_ptr<CollectorOrchestrator> m_collector;
        EventBus m_bus;
        EventStore m_store;
        std::atomic<bool> m_running{false};
    };
} // namespace chromelab

#endif // _LABD_SERVER_H_
