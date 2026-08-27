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

#include "labd/daemon.h"

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    g_running.store(false);
}

namespace chromelab {
    LabDaemonImpl::LabDaemonImpl(const LabdConfig& config) :
        m_config(config),
        m_collector(std::make_unique<CollectorOrchestrator>(config, &m_bus)),
        m_services(std::make_unique<ServiceManager>(&m_bus)),
        m_store(10000) {

        if (!config.events_dir.empty()) {
            m_store.EnableDisk(config.events_dir);
        }

        if (config.ai_enabled) {
            m_ai = std::make_unique<AiEngine>(config.models_dir, config.ai_max_ram, &m_bus);
            if (!config.ai_default_model.empty()) {
                m_ai->Load(config.ai_default_model);
            }
        }

        if (config.wg_enabled) {
            m_wg = std::make_unique<WireGuardManager>(config.wg_interface, config.wg_listen_port, config.wg_cidr, config.wg_dns, &m_bus);
        }

        if (config.ts_enabled) {
            m_ts = std::make_unique<TailscaleManager>(&m_bus);
        }

        if (config.dns_enabled) {
            m_dns = std::make_unique<DnsServer>(config.dns_domain, &m_bus);
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

        // Start HTTP dashboard
        m_httpd = std::make_unique<HttpServer>(m_config.http_port, m_config.web_dir, m_collector.get(), &m_store, m_services.get());

        if (m_httpd->Start()) {
            std::cout << "dashboard on http://localhost:" << m_config.http_port << "\n";
        } else {
            std::cerr << "warning: HTTP server failed to start on port " << m_config.http_port << "\n";
        }

        // Start DNS server
        if (m_dns) {
            if (m_dns->Start()) {
                std::cout << "DNS server started\n";
            } else {
                std::cerr << "warning: DNS server failed to start\n";
            }
        }

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
            if (m_httpd && m_wg) {
                m_httpd->SetWireGuardActive(m_wg->GetStatus().state() == WG_STATE_UP);
            }
            if (m_httpd && m_ts) {
                m_httpd->SetTailscaleActive(m_ts->IsUp());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        Stop();
    }

    void LabDaemonImpl::Stop(void) {
        if (m_httpd) {
            m_httpd->Stop();
        }

        if (m_dns) {
            m_dns->Stop();
        }

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

        auto services = m_services->ListServices();
        int running   = 0;
        for (const auto& svc : services) {
            if (svc.state() == SERVICE_STATE_RUNNING) running++;
        }

        resp->set_version("0.1.0");
        resp->set_hostname("chromelab");
        resp->set_uptime_seconds(snap.uptime().uptime_seconds());
        resp->set_ai_loaded(m_ai && m_ai->GetStatus().status() == MODEL_STATUS_READY);
        resp->set_wireguard_active(m_wg && m_wg->GetStatus().state() == WG_STATE_UP);
        resp->set_tailscale_active(m_ts && m_ts->IsUp());
        resp->set_services_running(running);
        resp->set_services_total(static_cast<int32_t>(services.size()));

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

        std::ifstream cpuinfo("/proc/cpuinfo");
        int core_count = 0;
        std::string cpu_model;
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.compare(0, 8, "model name") == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    cpu_model = line.substr(pos + 2);
                }
            }
            if (line.compare(0, 9, "processor") == 0) {
                ++core_count;
            }
        }

        resp->set_cpu_model(cpu_model);
        resp->set_cpu_cores(core_count);
        resp->set_arch("x86_64");

        auto snap = m_collector->GetSnapshot();
        resp->set_total_ram_bytes(snap.memory().total_bytes());

        int64_t total_disk = 0;
        for (const auto& fs : snap.disk().filesystems()) {
            total_disk += fs.total_bytes();
        }
        resp->set_total_disk_bytes(total_disk);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ValidateConfig(grpc::ServerContext* ctx, const Config* req, ValidateResponse* resp) {
        resp->set_valid(true);

        std::string raw = req->raw_toml();
        if (raw.empty()) {
            return grpc::Status::OK;
        }

        // Parse the provided TOML and check for basic errors
        try {
            toml::table tbl = toml::parse(raw);

            // Validate daemon section
            if (auto* daemon = tbl["daemon"].as_table()) {
                if (auto* node = daemon->get("http_port")) {
                    if (auto port = node->as_integer()) {
                        if (*port < 1 || *port > 65535) {
                            resp->set_valid(false);
                            resp->add_errors("daemon.http_port must be 1-65535");
                        }
                    }
                }
            }

            // Validate wireguard section
            if (auto* wg = tbl["wireguard"].as_table()) {
                if (auto* node = wg->get("listen_port")) {
                    if (auto port = node->as_integer()) {
                        if (*port < 1 || *port > 65535) {
                            resp->set_valid(false);
                            resp->add_errors("wireguard.listen_port must be 1-65535");
                        }
                    }
                }
            }

            // Validate ai section
            if (auto* ai_tbl = tbl["ai"].as_table()) {
                if (auto* node = ai_tbl->get("max_ram")) {
                    if (auto ram = node->as_integer()) {
                        if (*ram < 134217728) { // 128 MB min
                            resp->add_warnings("ai.max_ram is very low, models may fail to load");
                        }
                    }
                }
            }

        } catch (const toml::parse_error& e) {
            resp->set_valid(false);
            resp->add_errors(std::string("TOML parse error: ") + e.what());
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::Reboot(grpc::ServerContext* ctx, const RebootRequest* req, Empty* resp) {
        Event ev;
        ev.set_category(EVENT_CATEGORY_SYSTEM);
        ev.set_severity(SEVERITY_WARNING);
        ev.set_source("labd");
        ev.set_message("System reboot requested: " + req->reason());
        m_store.Append(&ev);
        m_bus.Emit(ev);

        int delay = req->delay_seconds();
        if (delay <= 0) {
            delay = 1;
        }

        // Schedule reboot in background thread
        std::thread([delay]() {
            std::this_thread::sleep_for(std::chrono::seconds(delay));
            int ret = system("reboot");
            (void)ret;
        }).detach();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::Shutdown(grpc::ServerContext* ctx, const Empty* req, Empty* resp) {
        Event ev;
        ev.set_category(EVENT_CATEGORY_SYSTEM);
        ev.set_severity(SEVERITY_WARNING);
        ev.set_source("labd");
        ev.set_message("Daemon shutdown requested via RPC");
        m_store.Append(&ev);
        m_bus.Emit(ev);

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
        auto sub = m_bus.Subscribe(EVENT_CATEGORY_UNSPECIFIED, [&](const Event& event) {
            Event copy = event;
            writer->Write(copy);
        });

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
        auto services = m_services->ListServices();
        for (auto& svc : services) {
            *resp->add_services() = std::move(svc);
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceInfo* resp) {
        auto info = m_services->GetService(req->name());
        if (!info) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "service not found: " + req->name());
        }

        *resp = std::move(*info);

        return grpc::Status::OK;
    }

    static grpc::Status to_grpc(const ServiceManager::ActionResult& r) {
        if (!r.error.empty()) {
            return grpc::Status(grpc::StatusCode::INTERNAL, r.error);
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        auto r = m_services->Start(req->name());
        resp->set_name(r.name);
        resp->set_previous_state(r.previous_state);
        resp->set_current_state(r.current_state);
        if (!r.error.empty()) {
            resp->set_error(r.error);
        }

        return to_grpc(r);
    }

    grpc::Status LabDaemonImpl::StopService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        auto r = m_services->Stop(req->name());
        resp->set_name(r.name);
        resp->set_previous_state(r.previous_state);
        resp->set_current_state(r.current_state);
        if (!r.error.empty()) {
            resp->set_error(r.error);
        }

        return to_grpc(r);
    }

    grpc::Status LabDaemonImpl::RestartService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        auto r = m_services->Restart(req->name());
        resp->set_name(r.name);
        resp->set_previous_state(r.previous_state);
        resp->set_current_state(r.current_state);
        if (!r.error.empty()) {
            resp->set_error(r.error);
        }

        return to_grpc(r);
    }

    grpc::Status LabDaemonImpl::EnableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        auto r = m_services->Enable(req->name());
        resp->set_name(r.name);
        resp->set_previous_state(r.previous_state);
        resp->set_current_state(r.current_state);
        if (!r.error.empty()) {
            resp->set_error(r.error);
        }

        return to_grpc(r);
    }

    grpc::Status LabDaemonImpl::DisableService(grpc::ServerContext* ctx, const ServiceRequest* req, ServiceResponse* resp) {
        auto r = m_services->Disable(req->name());
        resp->set_name(r.name);
        resp->set_previous_state(r.previous_state);
        resp->set_current_state(r.current_state);
        if (!r.error.empty()) {
            resp->set_error(r.error);
        }

        return to_grpc(r);
    }

    grpc::Status LabDaemonImpl::CheckServiceHealth(grpc::ServerContext* ctx, const ServiceRequest* req, HealthStatus* resp) {
        *resp = m_services->CheckHealth(req->name());

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetAIStatus(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) {
        if (!m_ai) {
            resp->set_status(ModelStatus::MODEL_STATUS_UNLOADED);
            resp->set_error("AI not enabled in config");
            return grpc::Status::OK;
        }
        *resp = m_ai->GetStatus();
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListModels(grpc::ServerContext* ctx, const Empty* req, ModelsList* resp) {
        if (!m_ai) {
            return grpc::Status::OK;
        }

        auto models = m_ai->ListModels();
        for (auto& m : models) {
            *resp->add_models() = std::move(m);
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::LoadModel(grpc::ServerContext* ctx, const ModelRequest* req, AIStatus* resp) {
        if (!m_ai) {
            resp->set_status(ModelStatus::MODEL_STATUS_ERROR);
            resp->set_error("AI not enabled in config");
            return grpc::Status::OK;
        }

        *resp = m_ai->Load(req->model_name());
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::UnloadModel(grpc::ServerContext* ctx, const Empty* req, AIStatus* resp) {
        if (!m_ai) {
            resp->set_status(ModelStatus::MODEL_STATUS_UNLOADED);
            return grpc::Status::OK;
        }

        *resp = m_ai->Unload();
        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ChatComplete(grpc::ServerContext* ctx, const ChatRequest* req, ChatResponse* resp) {
        if (!m_ai) {
            resp->set_content("AI not enabled in config");
            return grpc::Status::OK;
        }

        int32_t max_tokens = req->max_tokens() > 0 ? req->max_tokens() : 512;
        double temp        = req->temperature() > 0 ? req->temperature() : 0.7;

        auto result = m_ai->ChatComplete({ req->messages().begin(), req->messages().end() }, max_tokens, temp);

        resp->set_content(result.content);
        resp->set_tokens_generated(result.tokens_generated);
        resp->set_prompt_eval_ms(result.prompt_eval_ms);
        resp->set_eval_ms(result.eval_ms);

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::StreamChat(grpc::ServerContext* ctx, const ChatRequest* req, grpc::ServerWriter<ChatToken>* writer) {
        if (!m_ai) {
            ChatToken tok;
            tok.set_token("AI not enabled in config");
            tok.set_done(true);
            writer->Write(tok);
            return grpc::Status::OK;
        }

        int32_t max_tokens = req->max_tokens() > 0 ? req->max_tokens() : 512;
        double temp        = req->temperature() > 0 ? req->temperature() : 0.7;

        m_ai->StreamChat({ req->messages().begin(), req->messages().end() }, max_tokens, temp, [&](const std::string& token, bool done) -> bool {
            ChatToken tok;
            tok.set_token(token);
            tok.set_done(done);
            return writer->Write(tok);
        });

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::GetWireGuardStatus(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) {
        if (!m_wg) {
            resp->set_state(WG_STATE_DOWN);
            resp->set_error("WireGuard not enabled in config");
            return grpc::Status::OK;
        }

        *resp = m_wg->GetStatus();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::WireGuardUp(grpc::ServerContext* ctx, const WGRequest* req, WGStatus* resp) {
        if (!m_wg) {
            resp->set_state(WG_STATE_DOWN);
            resp->set_error("WireGuard not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_wg->Up();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::WireGuardDown(grpc::ServerContext* ctx, const Empty* req, WGStatus* resp) {
        if (!m_wg) {
            resp->set_state(WG_STATE_DOWN);
            resp->set_error("WireGuard not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_wg->Down();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::AddPeer(grpc::ServerContext* ctx, const AddPeerRequest* req, WGStatus* resp) {
        if (!m_wg) {
            resp->set_state(WG_STATE_DOWN);
            resp->set_error("WireGuard not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_wg->AddPeer(req->name(), req->public_key(), req->allowed_ips(), req->endpoint());

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::RemovePeer(grpc::ServerContext* ctx, const RemovePeerRequest* req, WGStatus* resp) {
        if (!m_wg) {
            resp->set_state(WG_STATE_DOWN);
            resp->set_error("WireGuard not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_wg->RemovePeer(req->public_key());

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::ListPeers(grpc::ServerContext* ctx, const Empty* req, PeersList* resp) {
        if (!m_wg) {
            return grpc::Status::OK;
        }

        auto peers = m_wg->ListPeers();
        for (auto& p : peers) {
            *resp->add_peers() = std::move(p);
        }

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::TailscaleStatus(grpc::ServerContext* ctx, const TSRequest* req, TSStatus* resp) {
        if (!m_ts) {
            resp->set_state(TS_STATE_DOWN);
            resp->set_error("Tailscale not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_ts->GetStatus();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::TailscaleUp(grpc::ServerContext* ctx, const TSLoginRequest* req, TSStatus* resp) {
        if (!m_ts) {
            resp->set_state(TS_STATE_DOWN);
            resp->set_error("Tailscale not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_ts->Up(req->authkey());

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::TailscaleDown(grpc::ServerContext* ctx, const Empty* req, TSStatus* resp) {
        if (!m_ts) {
            resp->set_state(TS_STATE_DOWN);
            resp->set_error("Tailscale not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_ts->Down();

        return grpc::Status::OK;
    }

    grpc::Status LabDaemonImpl::TailscaleIP(grpc::ServerContext* ctx, const Empty* req, TSStatus* resp) {
        if (!m_ts) {
            resp->set_state(TS_STATE_DOWN);
            resp->set_error("Tailscale not enabled in config");

            return grpc::Status::OK;
        }

        *resp = m_ts->GetIP();

        return grpc::Status::OK;
    }
} // namespace chromelab
