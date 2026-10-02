#pragma once

#include <filesystem>

namespace constants
{
    enum class SystemOS
    {
        Windows,
        MacOS,
        Linux,
        FreeBSD,
        Unix,
        Unknown
    };

    SystemOS get_os();
    std::filesystem::path get_data_path();
    extern const std::filesystem::path wal_file;
}
