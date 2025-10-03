# Minecraft C++ Server - Logging System

This document describes the comprehensive logging system replacement that was implemented to replace all `std::cout` and `std::cerr` statements with a structured, thread-safe logging framework.

## Overview

The logging system provides:
- **Structured logging** with categories (NETWORK vs GAMEINFO)
- **Log levels** (DEBUG, INFO, WARN, ERROR)
- **Source identification** for easier debugging
- **Automatic timestamping** and formatting
- **File-based persistence** with organized output
- **Thread-safe operation**
- **TUI integration** ready

## Usage Examples

### Network Events
```cpp
// Log network events
g_logger->logNetwork(INFO, "Client connected from 192.168.1.100", "NetworkManager");
g_logger->logNetwork(ERROR, "Failed to authenticate client", "AuthManager");
g_logger->logNetwork(WARN, "Connection timeout for client on socket 45", "NetworkManager");
g_logger->logNetwork(DEBUG, "Packet received: size=128 bytes, type=handshake", "PacketHandler");
```

### Game/Server Events
```cpp
// Log game events  
g_logger->logGameInfo(INFO, "Player 'Steve' joined the server", "PlayerManager");
g_logger->logGameInfo(WARN, "Server TPS dropped to 18.5", "PerformanceMonitor");
g_logger->logGameInfo(ERROR, "Failed to save player data for 'Alex'", "DataManager");
g_logger->logGameInfo(DEBUG, "Chunk loaded: x=16, z=32", "WorldManager");
```

## Log Levels

| Level | Purpose | Example Use Case |
|-------|---------|------------------|
| `DEBUG` | Detailed diagnostic information | Packet parsing details, memory allocations |
| `INFO` | General information | Player joins/leaves, server status |
| `WARN` | Warning conditions | Performance degradation, recoverable errors |
| `ERROR` | Error conditions | Connection failures, critical errors |

## Log Categories

### NETWORK Category
Used for all network-related events:
- Client connections/disconnections
- Packet processing
- Network errors
- Authentication events
- Protocol handling

### GAMEINFO Category  
Used for game logic and server management:
- Player management
- World operations
- Server lifecycle
- Performance monitoring
- Configuration changes

## File Organization

The logging system creates timestamped log files in the `logs/` directory:

```
logs/
├── 2024-01-15_14-30-25/
│   ├── network-2024-01-15_14-30-25.txt      # All network events
│   └── gameinfo-2024-01-15_14-30-25.txt     # All game/server events
└── 2024-01-15_15-45-10/
    ├── network-2024-01-15_15-45-10.txt
    └── gameinfo-2024-01-15_15-45-10.txt
```

## Log Format

Each log entry follows this format:
```
[HH:MM:SS.mmm] [LEVEL] [SOURCE] MESSAGE
```

Example:
```
[14:30:25.123] [INFO]  [NetworkManager] Client connected from 192.168.1.100
[14:30:25.456] [ERROR] [AuthManager] Failed to authenticate client
[14:30:26.789] [WARN]  [PerformanceMonitor] Server TPS dropped to 18.5
```

## Integration Points

### Initialization
The global logger is initialized in the server startup:
```cpp
initializeGlobalLogger();
```

### Thread Safety
The logging system is fully thread-safe and handles concurrent writes from:
- Network receiver threads
- Network sender threads
- Worker threads
- Main server thread

### TUI Integration
The logging system supports TUI callbacks for real-time display of GAMEINFO logs in the console interface.

## Replaced Components

The following files had their logging statements converted:

### Core Server Files
- `src/server.cpp` - Server lifecycle, player management
- `src/main.cpp` - Application entry point

### Networking Files  
- `src/networking/networkQueuer.cpp` - Connection management
- `src/networking/networkWorker.cpp` - Packet processing
- `src/networking/packet/handshake.cpp` - Handshake protocol
- `src/networking/packet/ping.cpp` - Ping/pong handling
- `src/networking/packet/status.cpp` - Server status requests

### Protocol Files
- `src/packet.cpp` - Low-level packet parsing

### Logger Implementation
- `src/logger.cpp` - Core logging implementation
- `include/logger.hpp` - Logger interface

## Migration Summary

### Before (Old System)
```cpp
std::cout << "[Network Manager] New connection accepted on socket " << socket << std::endl;
std::cerr << "[Server] Error: Could not load configuration" << std::endl;
```

### After (New System)
```cpp
g_logger->logNetwork(INFO, "New connection accepted on socket " + std::to_string(socket), "NetworkManager");
g_logger->logGameInfo(ERROR, "Could not load configuration", "Server");
```

## Performance Characteristics

- **Asynchronous writing**: Log entries are queued and written by a background thread
- **Memory efficient**: Minimal memory overhead per log entry
- **File I/O optimized**: Buffered writes with periodic flushing
- **Thread contention minimized**: Lock-free queuing where possible

## Testing

A demonstration program `logging_demo.cpp` is available to showcase the logging system capabilities:

```bash
g++ -std=c++17 -I include -pthread logging_demo.cpp src/logger.cpp -o logging_demo
./logging_demo
```

## Future Enhancements

Potential future improvements:
- Log rotation based on file size or time
- Network-based log aggregation
- Log filtering and search capabilities
- Integration with external monitoring systems
- Compression of archived log files

## Troubleshooting

### Common Issues

1. **Logger not initialized**: Ensure `initializeGlobalLogger()` is called before any logging
2. **Permission errors**: Check write permissions for the `logs/` directory
3. **Missing log files**: Verify the logger initialization completed successfully

### Debug Mode

For verbose logging output, build in debug mode to enable DEBUG level messages:
```bash
make debug
```

This comprehensive logging system provides a solid foundation for monitoring, debugging, and maintaining the Minecraft C++ server with professional-grade logging capabilities.