# mc-server

A Minecraft Java Edition server implementation written in C++20.

**Target Version:** Minecraft 1.21.10 (Protocol 773)

## Prerequisites

- **CMake** 3.21 or higher
- **Ninja** build system (recommended)
- **C++20** compatible compiler:
  - GCC 11+
  - Clang 14+
  - MSVC 2022+
- **zlib** (auto-downloaded if not found on system)

## Building

### Quick Start

```bash
# Configure and build in release mode
cmake --workflow --preset release

# Or in debug mode
cmake --workflow --preset debug
```

### Step-by-Step

```bash
# Configure the project
cmake --preset release    # or: cmake --preset debug

# Build
cmake --build --preset release    # or: cmake --build --preset debug
```

### Available Presets

| Preset | Description |
|--------|-------------|
| `debug` | Debug build with no optimizations, debug symbols |
| `release` | Optimized release build |
| `relwithdebinfo` | Optimized build with debug symbols |

### Build Output

The compiled executable will be located at:
- Release: `build/release/mc-server` (or `mc-server.exe` on Windows)
- Debug: `build/debug/mc-server`

## Running

### Using the Run Scripts

The project includes helper scripts that set up a test environment:

**Linux/macOS:**
```bash
./scripts/run.sh [debug|release]
```

**Windows (PowerShell):**
```powershell
.\scripts\run.ps1 [-BuildType debug|release]
```

These scripts will:
1. Build the project if needed
2. Create a `test-server/` directory
3. Copy the executable, `config.json`, and `world/` folder
4. Run the server

### Manual Run

```bash
# After building
cd build/release
./mc-server
```

## Development

### LSP Support

CMake automatically generates `compile_commands.json` in the build directory for LSP support. You can symlink it to the project root:

```bash
# Linux/macOS
ln -s build/release/compile_commands.json .

# Windows (PowerShell as Admin)
New-Item -ItemType SymbolicLink -Path compile_commands.json -Target build\release\compile_commands.json
```

### Cleaning

```bash
# Clean build artifacts
cmake --build --preset release --target clean

# Full clean (remove build directory)
rm -rf build/release    # or: rm -rf build/debug
```

### Installing

```bash
# Install to default location (/usr/local on Linux)
cmake --install build/release

# Install to custom location
cmake --install build/release --prefix /path/to/install
```

## Project Structure

```
mc-cpp-server/
├── CMakeLists.txt          # Main CMake configuration
├── CMakePresets.json       # Build presets
├── cmake/                  # CMake modules
│   └── CompilerWarnings.cmake
├── scripts/                # Helper scripts
│   ├── run.sh             # Linux/macOS run script
│   └── run.ps1            # Windows run script
├── include/                # Header files
│   ├── data/              # Minecraft data definitions
│   ├── lib/               # Utility libraries
│   ├── network/           # Networking headers
│   └── world/             # World handling headers
├── src/                    # Source files
│   ├── data/              # Data handling
│   ├── entities/          # Entity implementations
│   ├── lib/               # Library implementations
│   ├── networking/        # Network stack
│   └── world/             # World management
├── config.json            # Server configuration
└── world/                 # Test world data
```

## Configuration

Server settings are stored in `config.json`:

```json
{
  "version": {
    "name": "1.21.10",
    "protocol": 773
  },
  "server": {
    "ip-address": "127.0.0.1",
    "port": 25565,
    "motd": "Minecraft C++ Server",
    "max-players": 20
  },
  "world": {
    "name": "world",
    "gamemode": "survival",
    "difficulty": "normal"
  }
}
```

## Dependencies

| Library | Purpose | Notes |
|---------|---------|-------|
| zlib | Data compression | Auto-downloaded via CMake FetchContent if not found |
| nlohmann/json | JSON parsing | Header-only, included in `include/lib/json.hpp` |

## License

See LICENSE file for details.
