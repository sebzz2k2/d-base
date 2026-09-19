#include <CLI/CLI.hpp>
#include <string>
#include "internal/logger.h"
#include "server/server.h"
#include "spdlog/spdlog.h"

int execute(int argc, char** argv)
{
    CLI::App app{};

    int flag_port = 5381;
    std::string logLevel = "info";

    app.add_option("--port", flag_port, "Port number")
        ->check(CLI::Range(1, 65535));
    app.add_option("--log-level", logLevel, "Log level")
        ->check(CLI::IsMember({"trace", "debug", "info", "warn", "err", "critical", "off"}));

    CLI11_PARSE(app, argc, argv);

    try {
        logging::initialize(logLevel);
        SPDLOG_INFO("Starting server on 0.0.0.0:{} with log level {}", flag_port, logLevel);
        return startServer(flag_port);
    } catch (const std::exception& error) {
        SPDLOG_ERROR("Server stopped with error: {}", error.what());
        return 1;
    }
}
