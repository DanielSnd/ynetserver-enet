# YNetServer

A high-performance middle-man server for the YarnNet Godot multiplayer module. YNetServer provides room-based multiplayer functionality with support for room creation, joining, RPC calls, packet forwarding, and web-based monitoring.

## Features

- **Room-based Multiplayer**: Create and join rooms with unique 6-character codes
- **Protocol Versioning**: Support for multiple game protocol versions
- **Password Protection**: Optional password-protected rooms
- **RPC System**: Remote Procedure Call support for server-to-client and client-to-client communication
- **Packet Forwarding**: Efficient packet routing between clients
- **Host Migration**: Automatic host reassignment when the original host leaves
- **Web Interface**: Real-time server status monitoring via HTTP
- **Room Management**: Set room properties like max players, privacy, and custom metadata
- **Client Management**: Kick clients, track connections, and manage client IDs
- **Compression**: Built-in ENET range coder compression for efficient bandwidth usage
- **Error Logging**: Comprehensive error logging and debugging support

## Architecture

YNetServer uses a hybrid architecture combining:
- **ENET**: For reliable UDP networking and peer-to-peer communication
- **Mongoose**: For the embedded web server and status monitoring
- **Custom Protocol**: Binary message serialization for efficient data transfer

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler (GCC, Clang, or MSVC)
- Linux, macOS, or Windows

### Build Instructions

1. Clone the repository:
```bash
git clone <repository-url>
cd ynetserver
```

2. Build using the provided script:
```bash
chmod +x build.sh
./build.sh
```

Or build manually:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The executable will be created as `build/ynetserver`.

## Usage

### Server Mode

Start the server with default settings:
```bash
./ynetserver
```

Start with debugging enabled:
```bash
./ynetserver --debug
```

The server will start on:
- **Game Port**: 7777 (ENET)
- **Web Interface**: 8077 (HTTP)

### Client Mode

Run in client mode for testing:
```bash
./ynetserver --client [server_address]
```

Example:
```bash
./ynetserver --client localhost:7777
```

### Command Line Options

- `--debug` or `--d`: Enable debugging output
- `--client` or `--c [address]`: Run in client mode
- `--protocol` or `--p [protocol]`: Set protocol version
- `--ip [address]`: Specify server address

## Message Types

YNetServer supports a comprehensive set of message types:

### Room Management
- `ROOM_CREATE`: Create a new room
- `ROOM_JOIN`: Join an existing room
- `ROOM_LEAVE`: Leave current room
- `CREATE_ROOM_WITH_PASSWORD`: Create password-protected room
- `JOIN_ROOM_WITH_PASSWORD`: Join password-protected room
- `JOIN_OR_CREATE_ROOM`: Join existing room or create new one
- `JOIN_OR_CREATE_ROOM_RANDOM`: Join random available room

### Communication
- `MESSAGE`: Broadcast message to room
- `RPC_TO_SERVER`: Send RPC to server
- `RPC_TO_CLIENTS`: Send RPC to all clients in room
- `RPC_TO_CLIENT`: Send RPC to specific client
- `PACKET_TO_SERVER`: Send packet to server
- `PACKET_TO_CLIENTS`: Send packet to all clients
- `PACKET_TO_CLIENT`: Send packet to specific client

### Room Settings
- `SET_ROOM_PASSWORD`: Set room password
- `SET_MAX_PLAYERS`: Set maximum players
- `SET_ROOM_PRIVATE`: Set room privacy
- `SET_HOST_MIGRATION`: Enable/disable host migration
- `SET_ROOM_NAME`: Set room name
- `SET_EXTRA_INFO`: Set custom room metadata

### Information
- `GET_ROOM_LIST`: Get list of available rooms
- `GET_ROOM_INFO`: Get current room information
- `CONFIRM_ROOM_CREATION`: Room creation confirmation
- `CONFIRM_ROOM_JOIN`: Room join confirmation
- `PLAYER_JOINED`: Player joined notification
- `PLAYER_LEFT`: Player left notification
- `HOST_LEFT`: Host left notification

### Management
- `KICK_CLIENT`: Kick a client from room
- `CONFIRM_CONNECTION`: Connection confirmation

## Room System

### Room Codes
- 6-character alphanumeric codes
- Automatically generated or user-specified

### Room Properties
- **Protocol Version**: Ensures compatibility between clients
- **Max Players**: Configurable player limit (default: 4)
- **Password Protection**: Optional password with hash verification
- **Privacy**: Public or private rooms
- **Host Migration**: Automatic host reassignment
- **Custom Metadata**: Room name and extra information

### Room Lifecycle
1. **Creation**: Room created with unique code and properties
2. **Joining**: Clients join with protocol version matching
3. **Active**: Room serves as communication hub
4. **Host Migration**: Automatic if enabled when host leaves
5. **Cleanup**: Room removed when empty or host leaves without migration

## Web Interface

Access the web interface at `http://localhost:8077` to view:
- Total rooms and players
- Active room details
- Player lists per room
- Real-time status updates (auto-refresh every 5 seconds)

## Configuration

### Server Settings
- **Max Clients**: 4000 concurrent connections
- **Ping Interval**: 2000ms
- **Timeout Settings**: 32 timeouts, 10-30 second ranges
- **Compression**: ENET range coder enabled

### Room Settings
- **Min Room ID**: 1000
- **Max Room ID**: 65535
- **Min Client ID**: 1000
- **Max Client ID**: 65535

## Error Handling

The server includes comprehensive error handling:
- **Connection Errors**: Automatic cleanup of disconnected clients
- **Protocol Mismatches**: Rejection of incompatible clients
- **Invalid Messages**: Error logging and client notification
- **Room Conflicts**: Duplicate room code prevention
- **Resource Exhaustion**: Client ID and room ID management

## Logging

- **Console Output**: Real-time server events and debugging
- **Error Log**: Persistent error logging to `error.log`
- **Debug Mode**: Enhanced logging with `--debug` flag

## Performance

- **High Concurrency**: Supports up to 4000 simultaneous clients
- **Efficient Networking**: ENET-based UDP with reliability options
- **Memory Management**: Automatic cleanup of disconnected clients
- **Compression**: Built-in data compression for bandwidth optimization

## Integration with YarnNet

YNetServer is designed to work seamlessly with the YarnNet Godot multiplayer module:
- **Protocol Compatibility**: Supports YarnNet's binary message format
- **Room Integration**: Provides room-based multiplayer infrastructure
- **Packet Routing**: Efficient packet forwarding between clients

## Development

### Project Structure
```
ynetserver/
├── main.cpp              # Main entry point
├── ynetserver.h/cpp      # Core server implementation
├── ynetclient.h/cpp      # Client implementation
├── message.h/cpp         # Message definitions and serialization
├── room.h                # Room structure definitions
├── client.h              # Client structure definitions
├── thirdparty/           # Third-party libraries
│   ├── enet.h            # ENET networking library
│   ├── mongoose.h/c      # Web server library
│   ├── json.hpp          # JSON parsing library
│   └── serialize.h       # Binary serialization utilities
├── CMakeLists.txt        # Build configuration
├── build.sh              # Build script
└── README.md             # This file
```

### Adding New Features
1. Define new message types in `message.h`
2. Implement message handlers in `ynetserver.cpp`
3. Update serialization if needed
4. Add appropriate error handling
5. Update documentation

## License

MIT License
