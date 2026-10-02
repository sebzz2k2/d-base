#include "internal/logger.h"

#include <stdexcept>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace logging
{

    void initialize(const std::string &level)
    {
        const auto parsedLevel = spdlog::level::from_str(level);
        if (parsedLevel == spdlog::level::off && level != "off")
        {
            throw std::invalid_argument("invalid log level: " + level);
        }

        auto logger = spdlog::stdout_color_mt("db");
        logger->set_level(parsedLevel);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%# %!] %v");
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::err);
    }

}
