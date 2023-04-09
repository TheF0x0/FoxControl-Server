/**
 * @author F0x0
 * @since 05/04/2023
 */

#pragma once

#include <string>
#include <string_view>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <httplib.h>
#include <kstd/types.hpp>

namespace fox {
    class Monitor;

    class Server;

    class Gateway final {
        httplib::SSLClient _client;
        Server& _server;
        std::string _address;
        kstd::u32 _port;
        kstd::u32 _update_rate;
        std::thread _thread;
        std::atomic_bool _is_running;
        std::string _certificate_path;
        std::string _password;
        std::string _session_password;
        mutable std::shared_mutex _session_password_mutex;
        Monitor* _monitor;

        static auto check_status(const httplib::Result& res) noexcept -> bool;

        static auto broadcast_is_online(Gateway* self, bool is_online) noexcept -> void;

        static auto broadcast_state(Gateway* self) noexcept -> void;

        static auto create_session(Gateway* self) noexcept -> void;

        static auto update_loop(Gateway* self) noexcept -> void;

        public:

        Gateway(Server& server, std::string address, kstd::u32 port, kstd::u32 update_rate, std::string certificate_path, std::string password) noexcept;

        ~Gateway() noexcept;

        auto reset_session() noexcept -> void;

        inline auto attach_monitor(Monitor* monitor) noexcept -> void {
            _monitor = monitor;
        }

        [[nodiscard]] inline auto is_running() const noexcept -> bool {
            return _is_running;
        }

        [[nodiscard]] inline auto get_address() const noexcept -> const std::string& {
            return _address;
        }

        [[nodiscard]] inline auto get_port() const noexcept -> kstd::u32 {
            return _port;
        }

        [[nodiscard]] inline auto get_update_rate() const noexcept -> kstd::u32 {
            return _update_rate;
        }

        [[nodiscard]] inline auto get_server() noexcept -> Server& {
            return _server;
        }

        [[nodiscard]] inline auto get_client() noexcept -> httplib::SSLClient& {
            return _client;
        }

        [[nodiscard]] inline auto get_session_password() const noexcept -> std::string {
            _session_password_mutex.lock_shared();
            auto password = _session_password;
            _session_password_mutex.unlock_shared();
            return password;
        }
    };
}