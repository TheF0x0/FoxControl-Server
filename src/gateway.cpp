/**
 * @author F0x0
 * @since 05/04/2023
 */

#include <spdlog/spdlog.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <exception>
#include "gateway.hpp"
#include "dto.hpp"
#include "monitor.hpp"
#include "server.hpp"

#define FOX_JSON_MIME_TYPE "application/json"

namespace fox {
    Gateway::Gateway(Server& server, std::string address, kstd::u32 port, kstd::u32 update_rate, std::string certificate_path, std::string password) noexcept:
            _client(address, static_cast<int>(port)),
            _server(server),
            _address(std::move(address)),
            _port(port),
            _update_rate(update_rate),
            _is_running(true),
            _certificate_path(std::move(certificate_path)),
            _password(std::move(password)),
            _monitor() {
        _thread = std::thread(update_loop, this);
    }

    Gateway::~Gateway() noexcept {
        _is_running = false;
        _thread.join();
    }

    auto Gateway::reset_session() noexcept -> void {
        _session_password_mutex.lock();
        _session_password.clear();
        _session_password_mutex.unlock();

        broadcast_is_online(this, false);
        broadcast_is_online(this, true);
        create_session(this);
    }

    auto Gateway::check_status(const httplib::Result& res) noexcept -> bool {
        if (!res) {
            spdlog::error("Could not session data: invalid response");
            return false;
        }

        const auto status = res->status;

        if (status == 200) {
            return true;
        }

        try {
            const auto res_body = nlohmann::json::parse(res->body);

            if (res_body.contains("error")) {
                spdlog::error("Could not fetch session data: code {}/{}", status, res_body["error"]);
            }
            else {
                throw std::runtime_error("Could not decode gateway error"); // Whoops, control flow abuse
            }
        }
        catch (const std::exception& error) {
            spdlog::error("Could not fetch session data: code {}/{}", status, error.what());
        }

        return false;
    }

    auto Gateway::update_loop(Gateway* self) noexcept -> void {
        using namespace std::chrono_literals;

        spdlog::info("Starting gateway client");
        auto& client = self->_client;

        client.set_ca_cert_path(self->_certificate_path);
        client.enable_server_certificate_verification(false);
        client.set_default_headers({std::make_pair("Cache-Control", "private,max-age=0")}); // https://developers.cloudflare.com/cache/about/cache-control/

        const auto address = self->_address;
        const auto port = static_cast<int>(self->_port);
        spdlog::info("Connecting to {}:{}", address, port);

        broadcast_is_online(self, true);

        if (!create_session(self)) {
            return;
        }

        auto& server = self->_server;

        while (self->_is_running) {
            auto req_body = nlohmann::json::object();
            req_body["password"] = self->_password;
            req_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

            const auto response = client.Post("/fetch", req_body.dump(), FOX_JSON_MIME_TYPE);

            if (!check_status(response)) {
                continue;
            }

            const auto res_body = nlohmann::json::parse(response->body);

            if (!res_body.is_object() || !res_body.contains("tasks")) {
                spdlog::warn("Malformed response body");
                continue;
            }

            const auto& tasks = res_body["tasks"];

            if (!tasks.is_array()) {
                spdlog::warn("Tasks list must be an array");
                continue;
            }

            auto* monitor = self->_monitor;

            if (monitor != nullptr) {
                monitor->log_gateway(fmt::format("Fetched {} tasks from endpoint", tasks.size()));
            }

            for (const auto& task: tasks) {
                dto::Task task_dto{};
                task_dto.deserialize(task);

                switch (task_dto.type) {
                    case dto::TaskType::POWER:
                        server.set_is_on(task_dto.power.is_on);
                        break;
                    case dto::TaskType::SPEED:
                        server.set_speed(task_dto.speed.speed);
                        break;
                    case dto::TaskType::MODE:
                        server.set_mode(task_dto.mode.mode);
                        break;
                }
            }

            broadcast_state(self);
            std::this_thread::sleep_for(std::chrono::milliseconds(self->_update_rate));
        }

        broadcast_is_online(self, false);
    }

    auto Gateway::broadcast_is_online(Gateway* self, bool is_online) noexcept -> void {
        auto& client = self->_client;

        auto req_body = nlohmann::json::object();
        req_body["password"] = self->_password;
        req_body["is_online"] = is_online;
        req_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        check_status(client.Post("/setonline", req_body.dump(), FOX_JSON_MIME_TYPE));
    }

    auto Gateway::broadcast_state(Gateway* self) noexcept -> void {
        auto& client = self->_client;
        auto& server = self->_server;

        dto::DeviceState state{};
        state.is_on = server.is_on();
        state.accepts_commands = server.accepts_commands();
        state.target_speed = server.get_target_speed();
        state.actual_speed = server.get_actual_speed();
        state.mode = server.get_mode();

        auto req_body = nlohmann::json::object();
        req_body["password"] = self->_password;
        req_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        auto state_obj = nlohmann::json::object();
        state.serialize(state_obj);
        req_body["state"] = state_obj;

        check_status(client.Post("/setstate", req_body.dump(), FOX_JSON_MIME_TYPE));
    }

    auto Gateway::create_session(Gateway* self) noexcept -> bool {
        auto req_body = nlohmann::json::object();
        req_body["password"] = self->_password;
        req_body["timestamp"] = static_cast<kstd::u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        const auto response = self->_client.Post("/newsession", req_body.dump(), FOX_JSON_MIME_TYPE);

        if (!check_status(response)) {
            spdlog::warn("Received invalid new session response");
            return false;
        }

        auto res_body = nlohmann::json::parse(response->body);

        if (!res_body.contains("password")) {
            spdlog::warn("Received invalid new session response");
            return false;
        }

        self->_session_password_mutex.lock();
        self->_session_password = res_body["password"];
        spdlog::info("Created session password: {}", self->_session_password);
        self->_session_password_mutex.unlock();

        return true;
    }
}