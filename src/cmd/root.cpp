#include <CLI/CLI.hpp>
#include <string>
#include "internal/logger.h"
#include "server/server.h"
#include "spdlog/spdlog.h"
#include <thread>

int execute(int argc, char **argv)
{
    CLI::App app{};

    int flag_port = 5381;
    const unsigned int processor_count = std::thread::hardware_concurrency();

    struct
    {
        unsigned flush_interval_ms = 50;
        unsigned int queue_capacity = 1000;
    } WALConfig;

    const unsigned int available_workers =
        processor_count > 1 ? processor_count - 1 : 1;
    int workers = static_cast<int>(available_workers);

    std::string logLevel = "info";

    app.add_option("--port", flag_port, "Port number")
        ->check(CLI::Range(1, 65535));
    app.add_option("--log-level", logLevel, "Log level")
        ->check(CLI::IsMember({"trace", "debug", "info", "warn", "err", "critical", "off"}));
    app.add_option("--workers", workers, "Number of workers");

    app.add_option("--wal-flush-interval-ms", WALConfig.flush_interval_ms, "WAL flush interval in milliseconds")
        ->check(CLI::Range(1, 10000));

    app.add_option("--wal-queue-capacity", WALConfig.queue_capacity, "Maximum WAL queue capacity")
        ->check(CLI::Range(1, 100000));

    CLI11_PARSE(app, argc, argv);

    try
    {
        logging::initialize(logLevel);
        if (static_cast<unsigned int>(workers) > available_workers)
        {
            SPDLOG_WARN(
                "Requested {} worker threads, but only {} are available "
                "({} CPU cores; 1 reserved for the main thread). "
                "Using {} worker threads instead.",
                workers,
                available_workers,
                processor_count,
                available_workers);
            workers = static_cast<int>(available_workers);
        }

        SPDLOG_INFO(
            "Starting server on 0.0.0.0:{} with log level {} and {} worker threads",
            flag_port,
            logLevel,
            workers);
        return startServer(flag_port);
    }
    catch (const std::exception &error)
    {
        SPDLOG_ERROR("Server stopped with error: {}", error.what());
        return 1;
    }
}
