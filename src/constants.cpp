// constants.cpp
#include "constants.h"
#include <stdexcept>
#include <filesystem>

namespace constants
{
    const std::filesystem::path wal_file = get_data_path() / "wal";
    SystemOS get_os()
    {
#if defined(_WIN32) || defined(_WIN64)
        return SystemOS::Windows;
#elif defined(__APPLE__) || defined(__MACH__)
        return SystemOS::MacOS;
#elif defined(__linux__)
        return SystemOS::Linux;
#elif defined(__FreeBSD__)
        return SystemOS::FreeBSD;
#elif defined(__unix__) || defined(__unix)
        return SystemOS::Unix;
#else
        return SystemOS::Unknown;
#endif
    }

    std::filesystem::path get_data_path()
    {
        switch (get_os())
        {
        case SystemOS::Linux:
            return "/var/lib/vaultic";
        }
        throw std::runtime_error("Unhandled operating system");
    }
}
