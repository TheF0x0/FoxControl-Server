/**
 * @author F0x0
 * @since 04/04/2023
 */

#pragma once

#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <kstd/types.hpp>
#include <atomic_queue/atomic_queue.h>
#include "dto.hpp"

struct SDL_Window;
union SDL_Event;

namespace fox
{
    constexpr kstd::usize NUM_SPEED_HISTORY_ENTRIES = 32;
    constexpr kstd::usize MAX_CONSOLE_BUFFER_SIZE = 256;

    class Server;

    class Gateway;

    class Monitor final
    {
        Server& _server;
        Gateway& _gateway;

        std::queue<std::function<void()>> _render_tasks;
        std::mutex _task_queue_mutex;
        std::atomic_bool _is_running;
        std::atomic_bool _is_close_requested;

        bool _is_mouse_down;

        std::string _session_display_password;
        bool _is_session_password_visible;

        bool _auto_power_state;
        kstd::i32 _current_slider_speed;
        kstd::i32 _previous_slider_speed;
        dto::Mode _current_mode;

        std::vector<kstd::f32> _speed_history;
        std::vector<kstd::f32> _speed_delta_history;
        kstd::i32 _previous_speed;
        kstd::i32 _current_speed;

        std::vector<std::string> _device_log_buffer;
        std::mutex _device_log_mutex;
        bool _device_log_auto_scroll;
        kstd::f32 _device_log_prev_scroll_y;
        kstd::f32 _device_log_curr_scroll_y;

        std::vector<std::string> _gateway_log_buffer;
        std::mutex _gateway_log_mutex;
        bool _gateway_log_auto_scroll;
        kstd::f32 _gateway_log_prev_scroll_y;
        kstd::f32 _gateway_log_curr_scroll_y;

        auto load_window_icon(SDL_Window* window) noexcept -> void;

        auto render_window(SDL_Window* window) noexcept -> void;

        auto populate_window() noexcept -> void;

        auto populate_controls() noexcept -> void;

        auto populate_device_log() noexcept -> void;

        auto populate_gateway_log() noexcept -> void;

        auto handle_event(SDL_Window* window, const SDL_Event& event) noexcept -> void;

        auto update_data() noexcept -> void;

        auto update_speed_if_needed() noexcept -> void;

        [[nodiscard]] auto get_device_log() noexcept -> std::string;

        [[nodiscard]] auto get_gateway_log() noexcept -> std::string;

        auto show_session_password() noexcept -> void;

        auto hide_session_password() noexcept -> void;

        template <typename F>
            requires(std::is_convertible_v<F, std::function<void()>>)
        inline auto enqueue_render_task(F&& task)
        {
            std::scoped_lock lock(_task_queue_mutex);
            _render_tasks.push(std::forward<F>(task));
        }

    public:
        Monitor(Server& server, Gateway& gateway) noexcept;

        ~Monitor() noexcept = default;

        auto run() noexcept -> kstd::Result<void>;

        inline auto set_slider_speed(kstd::i32 speed) noexcept -> void
        {
            enqueue_render_task([this, speed]
            {
                _current_slider_speed = speed;
                _previous_slider_speed = speed;
            });
        }

        [[nodiscard]] inline auto is_running() const noexcept -> bool
        {
            return _is_running;
        }

        inline auto request_close() noexcept -> void
        {
            _is_close_requested = true;
        }

        inline auto log_device(const std::string_view& s) noexcept -> void
        {
            if (!_is_running)
            {
                return;
            }

            std::scoped_lock lock(_device_log_mutex);

            if (_device_log_buffer.size() == MAX_CONSOLE_BUFFER_SIZE)
            {
                _device_log_buffer.erase(_device_log_buffer.begin());
            }

            _device_log_buffer.emplace_back(s);
        }

        inline auto log_gateway(const std::string_view& s) noexcept -> void
        {
            if (!_is_running)
            {
                return;
            }

            std::scoped_lock lock(_gateway_log_mutex);

            if (_gateway_log_buffer.size() == MAX_CONSOLE_BUFFER_SIZE)
            {
                _gateway_log_buffer.erase(_gateway_log_buffer.begin());
            }

            _gateway_log_buffer.emplace_back(s);
        }

        inline auto clear_device_log() noexcept -> void
        {
            std::scoped_lock lock(_device_log_mutex);
            _device_log_buffer.clear();
        }

        inline auto clear_gateway_log() noexcept -> void
        {
            std::scoped_lock lock(_gateway_log_mutex);
            _gateway_log_buffer.clear();
        }

        [[nodiscard]] inline auto get_server() noexcept -> Server&
        {
            return _server;
        }

        [[nodiscard]] inline auto get_gateway() noexcept -> Gateway&
        {
            return _gateway;
        }
    };
}
