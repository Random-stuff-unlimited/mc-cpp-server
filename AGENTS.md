# AGENTS.md - Minecraft C++ Server

Guidelines for AI coding agents working in this repository.

## Project Overview

This is a Minecraft Java Edition server implementation in C++20, targeting protocol version 773 (MC 1.21.10). Uses POSIX networking APIs with primary Linux/Unix support (Windows WIP).

## Build Commands

### Quick Build (Recommended)
```bash
cmake --workflow --preset release    # Full release build
cmake --workflow --preset debug      # Full debug build
```

### Step-by-Step Build
```bash
# Configure
cmake --preset release               # or: debug, relwithdebinfo

# Build
cmake --build --preset release       # or: debug, relwithdebinfo

# Clean
cmake --build --preset release --target clean
rm -rf build/release                 # Full clean
```

### Run the Server
```bash
# Linux/macOS
./scripts/run.sh release             # or: debug

# Windows
.\scripts\run.ps1 -BuildType release # or: debug
```

### Build Output Locations
- Release: `build/release/mc-server` (or `.exe` on Windows)
- Debug: `build/debug/mc-server`

## Testing

**No formal testing framework is configured.** The project does not use CTest or any unit testing library. Manual testing is done by running the server and connecting with a Minecraft client.

## Linting and Formatting

### clang-format
```bash
# Format all source files
find . -name '*.cpp' -o -name '*.hpp' -o -name '*.h' | \
    grep -v "include/lib/json.hpp" | \
    xargs clang-format -i --style=file

# Check single file
clang-format --dry-run --Werror path/to/file.cpp
```

### Key Format Settings (.clang-format)
- **Indent**: 4 spaces width, tabs only (UseTab: Always)
- **Column limit**: 150 characters
- **Braces**: Attach style (same line as statement)
- **Pointers**: Left-aligned (`int* ptr`, not `int *ptr`)
- **Includes**: Sorted and regrouped automatically

## Code Style Guidelines

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `Player`, `NetworkManager`, `LogManager` |
| Member variables | Leading underscore + camelCase | `_playerName`, `_socketFd`, `_config` |
| Methods | camelCase | `getPlayerName()`, `setPlayerState()` |
| Enums (type) | PascalCase | `PlayerState`, `LogLevel` |
| Enum values (scoped) | PascalCase | `PlayerState::Configuration` |
| Enum values (unscoped) | SCREAMING_CASE | `DEBUG`, `INFO`, `ERROR` |
| Namespaces | PascalCase | `World::Manager` |
| Constants/Macros | SCREAMING_CASE | `MAX_PLAYERS`, `PROTOCOL_VERSION` |

### Header Guards
Use both `#pragma once` and traditional guards:
```cpp
#pragma once
#ifndef FILENAME_HPP
#define FILENAME_HPP
// ...
#endif
```

### Include Order
1. Project headers (relative paths)
2. Standard library headers (alphabetically sorted)

```cpp
#include "config.hpp"
#include "network/networking.hpp"

#include <cstdint>
#include <string>
#include <vector>
```

### Class Structure
```cpp
class ClassName {
  private:
    Type _memberVariable;

  public:
    ClassName();
    ~ClassName();

    // Getters (const, inline when simple)
    Type getMember() const { return _member; }

    // Setters
    void setMember(Type value) { _member = value; }
};
```

### Error Handling
- Use exceptions (`std::runtime_error`, `std::exception`)
- Null pointer checks before dereferencing
- Try-catch blocks for error-prone operations
- Use the global logger: `g_logger->logGameInfo(ERROR, "message")`

### Pointer Style
- Left-aligned pointers: `int* ptr` not `int *ptr`
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) when ownership is involved

## Project-Specific Rules

From `.rules` file:
1. **Do not create markdown documentation** (except when explicitly requested)
2. **Verify code works** before iterating when creating from scratch
3. **Ask for direction** on data types for complex features
4. **Ask before adding libraries** - recommend but don't add without approval
5. **Do not modify code** unless explicitly asked

## Project Structure

```
mc-cpp-server/
├── include/           # Headers (.hpp)
│   ├── data/          # Minecraft registry data
│   ├── lib/           # Utility libraries (json, nbt, uuid)
│   ├── network/       # Networking headers
│   └── world/         # World handling
├── src/               # Source files (.cpp)
│   ├── data/          # Data handling
│   ├── entities/      # Entity implementations
│   ├── lib/           # Library implementations
│   ├── networking/    # Network stack
│   │   └── packet/    # Packet handlers
│   │       ├── clientbound/  # Server -> Client
│   │       └── serverbound/  # Client -> Server
│   └── world/         # World management
├── cmake/             # CMake modules
├── scripts/           # Build/run scripts
├── external/          # Git submodules
└── build/             # Build output (gitignored)
```

## Dependencies

| Library | Purpose | Notes |
|---------|---------|-------|
| zlib | Compression | Auto-fetched if not system-installed |
| nlohmann/json | JSON parsing | Header-only in `include/lib/json.hpp` |
| ws2_32 | Windows sockets | Windows only |

## Compiler Configuration

- **Standard**: C++20 (CMake: `CMAKE_CXX_STANDARD 20`)
- **Warnings**: Strict warnings enabled (see `cmake/CompilerWarnings.cmake`)
- **Build types**: Debug, Release, RelWithDebInfo

### Key Warnings Enabled
- `-Wall -Wextra -Wpedantic` (GCC/Clang)
- `-Wshadow`, `-Wconversion`, `-Wnull-dereference`
- `/W4 /permissive-` (MSVC)

## LSP Support

- `compile_commands.json` generated in build directory
- `.clangd` configured for C++23 analysis with strict include checking
- Copy or symlink `build/*/compile_commands.json` to project root for LSP

## Common Tasks

### Adding a New Packet Handler
1. Create header in `include/network/packet/`
2. Create implementation in `src/networking/packet/clientbound/` or `serverbound/`
3. Register in packet router (`src/networking/networkPacketRouter.cpp`)

### Adding a New Entity
1. Create header in `include/` or appropriate subdirectory
2. Create implementation in `src/entities/`
3. Follow existing patterns in `player.hpp`/`player.cpp`

## Notes

- Primary platform: Linux/Unix (epoll-based networking)
- Windows support: Work in progress
- Threading: Multi-threaded with worker, receiver, and sender threads
- Logging: Async logging to `logs/` directory via `g_logger`
