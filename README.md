# db

## Requirements

- CMake 4.2 or newer
- GCC/G++ or another C++ compiler with C++ support
- CLI11
- spdlog

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y cmake gcc g++ libcli11-dev libspdlog-dev
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/db
```

Options:

```text
--port PORT          Port number (default: 5381)
--log-level LEVEL    trace, debug, info, warn, err, critical, or off
--workers COUNT      Number of worker threads
```

Example:

```bash
./build/db --port 5381 --log-level info --workers 4
```
