/**
 * @author F0x0
 * @since 04/04/2023
 */

#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <parallel_hashmap/phmap.h>
#include <atomic_queue/atomic_queue.h>
#include <kstd/types.hpp>
#include <mutex>
#include "serial.hpp"
#include "dto.hpp"

namespace fox {
    constexpr char MESSAGE_ON = 'i';
    constexpr char MESSAGE_OFF = 'o';
    constexpr char MESSAGE_MODE = 'm';
    constexpr char MESSAGE_LOWER = 'l';
    constexpr char MESSAGE_HIGHER = 'h';

    constexpr int32_t MAX_SPEED = 32;
    constexpr int32_t MIN_SPEED = 0;
    constexpr dto::Mode MODES[] = {dto::Mode::DEFAULT};
    constexpr size_t NUM_MODES = sizeof(MODES);

    [[nodiscard]] inline auto get_mode_name(dto::Mode mode) noexcept -> std::string {
        switch (mode) {
            default:
                return "Default";
        }
    }

    struct DeviceState final {
        std::atomic_bool is_on;
        std::atomic<dto::Mode> mode;
        std::atomic_int32_t target_speed;
        std::atomic_int32_t actual_speed;
    };

    class Monitor;

    class Server final {
        serial::SerialConnection _connection;
        Monitor* _monitor;
        std::thread _tx_thread;
        std::thread _rx_thread;
        std::thread _command_thread;
        std::atomic_bool _is_running;
        std::atomic_bool _is_busy;
        DeviceState _device_state;
        phmap::parallel_flat_hash_map<std::string, std::function<void()>> _commands;
        std::queue<char> _message_queue;
        std::mutex _queue_mutex;

        static auto handle_feedback(Server* self, const std::string& feedback) noexcept -> void;

        static auto tx_loop(Server* self) noexcept -> void;

        static auto rx_loop(Server* self) noexcept -> void;

        static auto command_loop(Server* self) noexcept -> void;

        auto register_commands() noexcept -> void;

        public:

        Server(std::string device_name, kstd::u32 baud_rate) noexcept;

        ~Server() noexcept;

        auto set_speed(kstd::i32 speed) noexcept -> void;

        auto set_is_on(bool is_on) noexcept -> void;

        auto set_mode(dto::Mode mode) noexcept -> void;

        inline auto attach_monitor(Monitor* monitor) noexcept -> void {
            _monitor = monitor;
        }

        [[nodiscard]] inline auto accepts_commands() const noexcept -> bool {
            return _device_state.actual_speed == _device_state.target_speed;
        }

        [[nodiscard]] inline auto get_connection() noexcept -> serial::SerialConnection& {
            return _connection;
        }

        [[nodiscard]] inline auto is_running() const noexcept -> bool {
            return _is_running;
        }

        [[nodiscard]] inline auto is_busy() const noexcept -> bool {
            return _is_busy;
        }

        [[nodiscard]] inline auto get_actual_speed() const noexcept -> kstd::i32 {
            return _device_state.actual_speed;
        }

        [[nodiscard]] inline auto get_target_speed() const noexcept -> kstd::i32 {
            return _device_state.target_speed;
        }

        [[nodiscard]] inline auto is_on() const noexcept -> bool {
            return _device_state.is_on;
        }

        [[nodiscard]] inline auto get_mode() const noexcept -> dto::Mode {
            return _device_state.mode;
        }
    };
}