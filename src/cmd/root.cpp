#include <CLI/CLI.hpp>
#include <string>
#include "server/server.h"

int execute(int argc, char **argv)
{
    CLI::App app{};

    int flag_port = 5381;

    app.add_option("--port", flag_port, "port")->check(CLI::Range(1, 65535));

    CLI11_PARSE(app, argc, argv);

    std::cout << "Starting server on "
              << "0.0.0.0" << ":" << flag_port << '\n';
    
    return startServer(flag_port);
}