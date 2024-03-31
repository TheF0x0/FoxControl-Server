/**
 * @author F0x0
 * @since 04/04/2023
 */

#include <thread>
#include <string>
#include <iostream>
#include <kstd/errors.hpp>
#include <spdlog/spdlog.h>

#include "server.hpp"
#include "monitor.hpp"

namespace fox
{
    Server::Server(std::string device_name, kstd::u32 baud_rate) noexcept:
        _connection(serial::SerialConnection(std::move(device_name), serial::find_closest_baud_rate(baud_rate))),
        _monitor(),
        _is_running(true),
        _is_busy(false),
        _commands(),
        _message_queue()
    {
        register_commands();
        _rx_thread = std::thread(rx_loop, this);
        _tx_thread = std::thread(tx_loop, this);
        _command_thread = std::thread(command_loop, this);
    }

    Server::~Server() noexcept
    {
        _is_running = false;
        _tx_thread.join();
        _rx_thread.join();
        _command_thread.join();
    }

    auto Server::handle_feedback(Server* self, const std::string& feedback) noexcept -> void
    {
        auto& state = self->_device_state;

        if (feedback == "power_on")
        {
            state.actual_speed = 1;
        }
        else if (feedback == "power_off")
        {
            state.actual_speed = 0;
        }
        else if (feedback == "speed_up")
        {
            ++state.actual_speed;
        }
        else if (feedback == "speed_down")
        {
            --state.actual_speed;
        }
    }

    auto Server::tx_loop(Server* self) noexcept -> void
    {
        using namespace std::chrono_literals;
        spdlog::info("Starting serial TX thread");

        while (self->_is_running)
        {
            bool is_busy;
            {
                std::scoped_lock lock(self->_queue_mutex);
                is_busy = !self->_message_queue.empty();
            }
            if (is_busy)
            {
                char message;
                {
                    std::scoped_lock lock(self->_queue_mutex);
                    message = self->_message_queue.front();
                    self->_message_queue.pop();
                }

                if (!self->_connection.write(message))
                {
                    spdlog::warn("Dropped packet while sending, ignoring");
                }

                const auto log_message = fmt::format("[Host -> {}] {}", self->_connection.get_device_name(), message);
                spdlog::debug(log_message);

                auto* monitor = self->_monitor;

                if (monitor != nullptr)
                {
                    monitor->log_device(log_message);
                }
            }

            std::this_thread::sleep_for(1ms);
        }
    }

    auto Server::rx_loop(Server* self) noexcept -> void
    {
        using namespace std::chrono_literals;

        spdlog::info("Starting serial RX thread");
        std::vector<char> buffer;

        while (self->_is_running)
        {
            char current = '\0';
            buffer.clear();

            while (self->_connection.try_read(current))
            {
                if (current == '\n')
                {
                    break;
                }

                buffer.push_back(current);
            }

            if (!buffer.empty())
            {
                buffer.push_back('\0');

                auto feedback = std::string(buffer.data());
                if (feedback.ends_with('\r'))
                {
                    feedback = feedback.substr(0, feedback.size() - 1);
                }

                handle_feedback(self, feedback);

                const auto log_message = fmt::format("[{} -> Host] {}", self->_connection.get_device_name(), feedback);
                spdlog::debug(log_message);

                auto* monitor = self->_monitor;

                if (monitor != nullptr)
                {
                    monitor->log_device(log_message);
                }
            }

            std::this_thread::sleep_for(1ms);
        }
    }

    auto Server::command_loop(Server* self) noexcept -> void
    {
        spdlog::info("Starting command thread");
        std::string command;

        while (self->_is_running)
        {
            std::getline(std::cin, command);

            if (command.empty())
            {
                continue;
            }

            const auto itr = self->_commands.find(command);

            if (itr == self->_commands.end())
            {
                spdlog::info("Unrecognized command, try help");
                continue;
            }

            itr->second();
        }
    }

    auto Server::register_commands() noexcept -> void
    {
        _commands["help"] = [this]
        {
            for (const auto& pair : _commands)
            {
                spdlog::info(pair.first);
            }
        };

        _commands["exit"] = [this]
        {
            spdlog::info("Shutting down gracefully");

            if (_device_state.is_on)
            {
                std::scoped_lock lock(_queue_mutex);
                _message_queue.push(MESSAGE_OFF);
                _device_state.is_on = false;
            }

            if (_monitor != nullptr && _monitor->is_running())
            {
                _monitor->request_close();
            }

            _is_running = false;
        };

        _commands["power"] = [this]
        {
            spdlog::info("Requesting change of power status");
            set_is_on(!_device_state.is_on);
        };

        _commands["mode"] = [this]
        {
            if (!_device_state.is_on)
            {
                spdlog::info("This command only works if the machine is on");
                return;
            }

            spdlog::info("Requesting change of mode");
            set_mode(dto::Mode::DEFAULT); // TODO: implement mode selection
        };

        _commands["lower"] = [this]
        {
            const auto speed = get_target_speed();

            if (!_device_state.is_on || speed == 0)
            {
                spdlog::info("This command only works if the machine is on and if the speed is > 0");
                return;
            }

            spdlog::info("Requesting change of speed");
            const auto new_speed = speed - 1;
            set_speed(new_speed);
        };

        _commands["higher"] = [this]
        {
            const auto speed = get_target_speed();

            if (!_device_state.is_on || speed == MAX_SPEED)
            {
                spdlog::info("This command only works if the machine is on and the speed is < MAX_SPEED");
                return;
            }

            spdlog::info("Requesting change of speed");
            const auto new_speed = speed + 1;
            set_speed(new_speed);
        };
    }

    auto Server::set_speed(kstd::i32 speed) noexcept -> void
    {
        if (!_device_state.is_on && speed > 0)
        {
            set_is_on(true);
        }
        else if (_device_state.is_on && speed == 0)
        {
            set_is_on(false);
            return;
        }

        if (_device_state.target_speed < speed)
        {
            const auto diff = speed - _device_state.target_speed;

            for (auto i = 0; i < diff; ++i)
            {
                std::scoped_lock lock(_queue_mutex);
                _message_queue.push(MESSAGE_HIGHER);
            }
        }
        else if (_device_state.target_speed > speed)
        {
            const auto diff = _device_state.target_speed - speed;

            for (auto i = 0; i < diff; ++i)
            {
                std::scoped_lock lock(_queue_mutex);
                _message_queue.push(MESSAGE_LOWER);
            }
        }

        if (_monitor != nullptr)
        {
            _monitor->set_slider_speed(speed);
        }

        _device_state.target_speed = speed;
    }

    auto Server::set_is_on(bool is_on) noexcept -> void
    {
        if (_device_state.is_on == is_on)
        {
            return;
        }

        {
            std::scoped_lock lock(_queue_mutex);
            _message_queue.push(is_on ? MESSAGE_ON : MESSAGE_OFF);
        }

        _device_state.is_on = is_on;
        const auto new_speed = is_on ? 1 : 0;
        _device_state.target_speed = new_speed;

        if (_monitor != nullptr)
        {
            _monitor->set_slider_speed(new_speed);
        }
    }

    auto Server::set_mode(dto::Mode mode) noexcept -> void
    {
        if (!_device_state.is_on)
        {
            return;
        }

        _device_state.mode = mode;
    }
}
