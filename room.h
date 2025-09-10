#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include "thirdparty/enet.h"

// Room code character set
constexpr std::array<char, 25> ROOM_CODE_CHARS = {
    'A', 'B', 'C', 'D', 'E', 'F', 'R', 'T', 'P', 'X', 'Z', '9',
    'Q', 'H', 'J', 'Y', 'W', 'K', 'N', '1', '2', '3', '4', '5', '7'
};
constexpr std::array<char, 12> ROOM_CODE_CHARS_SQ = {
    'Z', 'T', 'Q', 'H', 'J', '1', '2', '3', '4', '5', '7', '9'
};

// Maximum room ID value (2 bytes)
constexpr uint16_t MAX_ROOM_ID = 65535;
constexpr uint16_t MAX_CLIENT_ID = 65535;

struct Room {
    uint16_t roomId;  // 2-byte room identifier
    std::string roomCode;
    uint32_t protocol;  // Protocol version as integer
    std::string roomName;
    std::string password;
    int32_t hashed_password;
    bool isPrivate;
    bool canHostMigrate = false;
    int maxPlayers;
    std::string extraInfo;
    ENetPeer* host;
    std::unordered_map<ENetPeer*, uint16_t> clients;
};
