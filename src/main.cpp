/**
 * @author F0x0
 * @since 04/04/2023
 */

#include <string>
#include <iostream>

#include <cxxopts/cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>

#include "server.hpp"
#include "monitor.hpp"
#include "gateway.hpp"

auto main(int num_args, char** args) -> int {
    spdlog::set_default_logger(spdlog::create<spdlog::sinks::stdout_color_sink_mt>("FoxControl"));
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%n] [%^---%L---%$] [thread %t] %v");

    cxxopts::Options option_spec("fox-control-server", "FoxControl Serial-to-REST bridge server");

    // @formatter:off
    option_spec.add_options()
       ("h,help", "Show this help dialog")
       ("d,device", "Specify the serial device to connect to", cxxopts::value<std::string>())
       ("r,rate", "Specify the serial IO baud rate", cxxopts::value<kstd::u32>()->default_value("19200"))
       ("a,address", "Specify the address of the HTTP gateway to connect to", cxxopts::value<std::string>())
       ("p,port", "Specify the port of the HTTP gateway to connect to", cxxopts::value<kstd::u32>()->default_value("443"))
       ("u,updaterate", "Specify the gateway fetch rate in milliseconds", cxxopts::value<kstd::u32>()->default_value("500"))
       ("c,certificate", "Specify the X509 certificate to use for gateway requests", cxxopts::value<std::string>()->default_value("./certificate.crt"))
       ("P,password", "Specify the password with which to authenticate against the gateway", cxxopts::value<std::string>())
       ("m,monitor", "Open the local monitor UI (Requires OpenGL 3.3)")
       ("V,verbose", "Enable verbose logging")
       ("v,version", "Show version information");
    // @formatter:on

    cxxopts::ParseResult options;

    try {
        options = option_spec.parse(num_args, args);
    }
    catch (const std::exception& error) {
        spdlog::error("Malformed arguments: {}", error.what());
        return 1;
    }

    if (options.count("help") > 0) {
        std::cout << option_spec.help() << std::endl;
        return 0;
    }

    if (options.count("verbose") > 0) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Verbose logging enabled");
    }

    if (options.count("version") > 0) {
        spdlog::info("FoxControl Serial Server Version 1.5");
        return 0;
    }

    const auto device = options["device"].as<std::string>();
    const auto baud_rate = options["rate"].as<kstd::u32>();
    fox::Server server(device, baud_rate);

    const auto gateway_address = options["address"].as<std::string>();
    const auto gateway_port = options["port"].as<kstd::u32>();
    const auto gateway_rate = options["updaterate"].as<kstd::u32>();
    const auto gateway_cert = options["certificate"].as<std::string>();
    const auto gateway_pass = options["password"].as<std::string>();
    fox::Gateway gateway(server, gateway_address, gateway_port, gateway_rate, gateway_cert, gateway_pass);

    if (options.count("monitor") > 0) {
        fox::Monitor monitor(server, gateway);

        if (const auto result = monitor.run(); !result.has_value()) {
            spdlog::error(result.error());
            return 1;
        }
    }

    while (server.is_running()) {
        std::this_thread::yield(); // Wait until server terminates
    }

    return 0;
}