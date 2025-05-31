#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <span>
#include <memory>
#include <iostream>
#include "thirdparty/enet.h"
#include "thirdparty/serialize.h"

enum MessageType {
    ERROR = 0,
    ROOM_CREATE = 1,
    ROOM_JOIN = 2,
    ROOM_LEAVE = 3,
    RPC_TO_SERVER = 4,
    RPC_TO_CLIENTS = 5,
    RPC_TO_CLIENT = 6,
    MESSAGE = 7,
    GET_ROOM_LIST = 8,
    CONFIRM_ROOM_CREATION = 9,
    CONFIRM_ROOM_JOIN = 10,
    PLAYER_JOINED = 11,
    ROOM_PEER_JOIN = 12,
    ROOM_PEER_LEFT = 13,
    SET_ROOM_PASSWORD = 14,
    SET_MAX_PLAYERS = 15,
    SET_ROOM_PRIVATE = 16,
    SET_HOST_MIGRATION = 17,
    SET_ROOM_NAME = 18,
    SET_EXTRA_INFO = 19,
    GET_ROOM_INFO = 20,
    PACKET_TO_SERVER = 21,
    PACKET_TO_CLIENTS = 22,
    PACKET_TO_CLIENT = 23,
    KICK_CLIENT = 24,
    JOIN_ROOM_WITH_PASSWORD = 25,
    JOIN_OR_CREATE_ROOM = 26,
    JOIN_OR_CREATE_ROOM_RANDOM = 27,
    PLAYER_LEFT = 28,
    CREATE_ROOM_WITH_PASSWORD = 29,
    HOST_LEFT = 30,
    CONFIRM_CONNECTION = 31,
    NOT_MESSAGE = 255
};

// The base message other messages extend
struct BaseMessage {
    uint8_t type; // 1-byte message type

    virtual ~BaseMessage() = default;

    virtual bool serialize(serialize::WriteStream & stream) { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) { return Serialize(stream); }

    // Template serialize handles both serializing and deserializing.
    // We can check if we're deserializing by checking if the stream
    // is reading with Stream::IsReading
    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        return true;
    }
};

struct ConfirmConnectionMessage : BaseMessage {
    uint16_t clientId; // 2-byte RPC type

    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_uint16( stream, clientId );
        return true;
    }
};

struct RPCMessage : BaseMessage {
    uint16_t rpcType; // 2-byte RPC type
    std::vector<uint8_t> data; // Variable-length data field
    
    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_uint16( stream, rpcType );

        if (Stream::IsReading) {
            // When reading, we need to read the size first
            uint32_t size = 0;
            serialize_uint32( stream, size );
            if (size > 0) {
                data.resize(size);
                serialize_bytes( stream, data.data(), static_cast<uint32_t>(size) );
            }
        } else {
            // When writing, we write the size first
            uint32_t size = static_cast<uint32_t>(data.size());
            serialize_uint32( stream, size );
            if (size > 0) {
                serialize_bytes( stream, data.data(), size );
            }
        }
        return true;
    }
};

struct Message : BaseMessage {
    std::string messageString;  // Combined data field

    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    static std::unique_ptr<BaseMessage> Deserialize(const uint8_t* data, size_t dataLength);
    
    void setMessageType(MessageType type) {
        this->type = static_cast<uint8_t>(type);
    }

    MessageType getMessageType() const {
        return static_cast<MessageType>(type);
    }

    // Helper methods for string data
    void setStringData(const std::string& str) {
        messageString = str;
    }

    std::string getStringData() const {
        return messageString;
    }

    template <typename Stream> bool Serialize( Stream & stream )
    {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, messageString );
        return true;
    }
};

// Message structure for room settings
struct RoomSettingMessage : BaseMessage {
    std::string settingValue; // Value of the setting
    
    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, settingValue );
        return true;
    }
};


// Message structure for room settings
struct CreateRoomWithPasswordMessage : BaseMessage {
    std::string roomCode; // Name of the setting
    std::string password; // Value of the setting
    
    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, roomCode );
        serialize_godot_string( stream, password );
        return true;
    }
};

// Message structure for room settings
struct JoinRoomWithPasswordMessage : BaseMessage {
    std::string roomCode; // Name of the setting
    std::string password; // Value of the setting

    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, roomCode );
        serialize_godot_string( stream, password );
        return true;
    }
};

struct ConfirmRoomJoinMessage : BaseMessage {
    std::string roomCode; // Name of the setting
    std::string jsonRoomPlayers; // Value of the setting
    
    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, roomCode );
        serialize_godot_string( stream, jsonRoomPlayers );
        return true;
    }
};

// Message structure for packet transfers
struct PacketMessage : BaseMessage {
    // typedef enum _ENetPacketFlag {
    //     ENET_PACKET_FLAG_RELIABLE            = (1 << 0), /** packet must be received by the target peer and resend attempts should be made until the packet is delivered */
    //     ENET_PACKET_FLAG_UNSEQUENCED         = (1 << 1), /** packet will not be sequenced with other packets */
    //     ENET_PACKET_FLAG_NO_ALLOCATE         = (1 << 2), /** packet will not allocate data, and user must supply it instead */
    //     ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT = (1 << 3), /** packet will be fragmented using unreliable (instead of reliable) sends if it exceeds the MTU */
    //     ENET_PACKET_FLAG_UNTHROTTLED         = (1 << 4), /** packet that was enqueued for sending unreliably should not be dropped due to throttling and sent if possible */
    //     ENET_PACKET_FLAG_SENT                = (1 << 8), /** whether the packet has been sent from all queues it has been entered into */
    // } ENetPacketFlag;
    uint8_t channel = 0; // 1-byte channel
    uint8_t reliabilityFlags = 1; // 1-byte flags
    uint32_t targetClientId; // 4-byte target client ID (0 for broadcast)
    std::vector<uint8_t> packetData; // Variable-length packet data

    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_uint8( stream, channel );
        serialize_uint8( stream, reliabilityFlags );
        serialize_uint32( stream, targetClientId );
        
        if (Stream::IsReading) {
            uint32_t size = 0;
            serialize_uint32( stream, size );
            if (size > 0) {
                packetData.resize(size);
                serialize_bytes( stream, packetData.data(), size );
            }
        } else {
            uint32_t size = static_cast<uint32_t>(packetData.size());
            serialize_uint32( stream, size );
            if (size > 0) {
                serialize_bytes( stream, packetData.data(), size );
            }
        }
        return true;
    }
};

// Message structure for room info requests
struct RoomInfoMessage : BaseMessage {
    std::string roomInfoAsJson; // Room info as JSON string
    
    virtual bool serialize(serialize::WriteStream & stream) override { return Serialize(stream); }
    virtual bool deserialize(serialize::ReadStream & stream) override { return Serialize(stream); }

    template <typename Stream> bool Serialize( Stream & stream ) {
        serialize_uint8( stream, type );
        serialize_godot_string( stream, roomInfoAsJson );
        return true;
    }
};

std::string EventTypeToString(ENetEventType type);

std::string MessageTypeToString(MessageType type);

std::string MessageTypeToString(uint8_t type);
