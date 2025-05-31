#pragma once

#include <string>

struct Client {
    uint16_t id;
    std::string currentRoom;
    uint16_t roomId;
    uint32_t protocolVersion;  // Protocol version as integer, defaults to 0
    bool debugging;
};