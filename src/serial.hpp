/**
 * @author F0x0
 * @since 04/04/2023
 */

#pragma once

#include <string_view>
#include <mutex>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <kstd/types.hpp>
#include <kstd/platform/platform.hpp>

namespace fox::serial {
    enum class BaudRate : kstd::u8 {
        // @formatter:off
        _50     = B50,
        _75     = B75,
        _110	= B110,
        _134	= B134,
        _150	= B150,
        _200	= B200,
        _300	= B300,
        _600	= B600,
        _1200	= B1200,
        _1800	= B1800,
        _2400	= B2400,
        _4800	= B4800,
        _9600	= B9600,
        _19200	= B19200,
        _38400	= B38400
        // @formatter:on
    };

    [[nodiscard]] inline auto find_closest_baud_rate(kstd::u32 rate) noexcept -> BaudRate {
        // @formatter:off
        if(rate <= 50)          return BaudRate::_50;
        else if(rate <= 75)     return BaudRate::_75;
        else if(rate <= 110)    return BaudRate::_110;
        else if(rate <= 134)    return BaudRate::_134;
        else if(rate <= 150)    return BaudRate::_150;
        else if(rate <= 200)    return BaudRate::_200;
        else if(rate <= 300)    return BaudRate::_300;
        else if(rate <= 600)    return BaudRate::_600;
        else if(rate <= 1200)   return BaudRate::_1200;
        else if(rate <= 1800)   return BaudRate::_1800;
        else if(rate <= 2400)   return BaudRate::_2400;
        else if(rate <= 4800)   return BaudRate::_4800;
        else if(rate <= 9600)   return BaudRate::_9600; // NOLINT
        else if(rate <= 19200)  return BaudRate::_19200;
        else if(rate <= 38400)  return BaudRate::_38400;
        else                    return BaudRate::_9600; // Default
        // @formatter:on
    }

    [[nodiscard]] inline auto to_baud_rate_count(BaudRate rate) noexcept -> uint32_t {
        switch(rate) { // @formatter:off
            case BaudRate::_50:     return 50;
            case BaudRate::_75:     return 75;
            case BaudRate::_110:    return 110;
            case BaudRate::_134:    return 134;
            case BaudRate::_150:    return 150;
            case BaudRate::_200:    return 200;
            case BaudRate::_300:    return 300;
            case BaudRate::_600:    return 600;
            case BaudRate::_1200:   return 1200;
            case BaudRate::_1800:   return 1800;
            case BaudRate::_2400:   return 2400;
            case BaudRate::_4800:   return 4800;
            case BaudRate::_9600:   return 9600;
            case BaudRate::_19200:  return 19200;
            case BaudRate::_38400:  return 38400;
            default:                return 9600; // Default
        } // @formatter:on
    }

    class SerialConnection final {
        std::string _device_name;
        int32_t _handle;
        BaudRate _baud_rate;

        public:

        SerialConnection(std::string device_name, BaudRate baud_rate) noexcept:
                _device_name(std::move(device_name)),
                _handle(::open(_device_name.data(), O_RDWR)),
                _baud_rate(baud_rate) {
            if (_handle == -1) {
                throw std::runtime_error(fmt::format("Could not open serial port: {}", kstd::platform::get_last_error()));
            }

            termios tty{};

            if (tcgetattr(_handle, &tty) != 0) {
                throw std::runtime_error(fmt::format("Could not retrieve port attributes: {}", kstd::platform::get_last_error()));
            }

            tty.c_cflag |= (CS8 | CLOCAL | CREAD);
            tty.c_cflag &= ~(CRTSCTS);
            tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            tty.c_oflag &= ~(OPOST | ONLCR);
            tty.c_cc[VTIME] = 10;
            tty.c_cc[VMIN] = 0;

            cfsetispeed(&tty, static_cast<speed_t>(baud_rate));
            cfsetospeed(&tty, static_cast<speed_t>(baud_rate));

            if (tcsetattr(_handle, TCSANOW, &tty) != 0) {
                throw std::runtime_error(fmt::format("Could not configure serial port: {}", kstd::platform::get_last_error()));
            }

            spdlog::info("Opened serial connection {}", _handle);
        }

        SerialConnection() noexcept:
                _device_name("Unknown"),
                _handle(-1),
                _baud_rate(BaudRate::_9600) {
        }

        SerialConnection(const SerialConnection& other) noexcept = default;

        SerialConnection(SerialConnection&& other) noexcept = default;

        ~SerialConnection() noexcept {
            ::close(_handle);
            spdlog::info("Closed serial connection {}", _handle);
        }

        inline auto operator =(const SerialConnection& other) noexcept -> SerialConnection& = default;

        inline auto operator =(SerialConnection&& other) noexcept -> SerialConnection& = default;

        [[nodiscard]] inline auto get_handle() const noexcept -> int32_t {
            return _handle;
        }

        [[nodiscard]] inline auto get_baud_rate() const noexcept -> BaudRate {
            return _baud_rate;
        }

        [[nodiscard]] inline auto get_device_name() const noexcept -> const std::string& {
            return _device_name;
        }

        [[nodiscard]] inline auto is_open() const noexcept -> bool {
            return _handle != -1;
        }

        template<typename M>
        requires(std::is_standard_layout_v<M>)
        inline auto write(const M& message) noexcept -> bool {
            constexpr auto message_size = sizeof(M);
            return ::write(_handle, &message, message_size) == message_size;
        }

        template<typename M>
        requires(std::is_standard_layout_v<M>)
        inline auto try_read(M& message) noexcept -> bool {
            constexpr auto message_size = sizeof(M);
            return ::read(_handle, &message, message_size) == message_size;
        }
    };
}