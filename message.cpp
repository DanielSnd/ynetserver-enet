#include "message.h"

std::string MessageTypeToString(uint8_t type) {
    return MessageTypeToString(static_cast<MessageType>(type));
}

std::unique_ptr<BaseMessage> Message::Deserialize(const uint8_t* data, size_t dataLength) {
    MessageType type = static_cast<MessageType>(data[0]);
    switch (type) {
        case MessageType::NOT_MESSAGE:
            {
                return std::make_unique<BaseMessage>();
            }
        case MessageType::JOIN_ROOM_WITH_PASSWORD:
            {
                auto message = std::make_unique<JoinRoomWithPasswordMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed JOIN_ROOM_WITH_PASSWORD\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::CONFIRM_ROOM_JOIN:
            {
                auto message = std::make_unique<ConfirmRoomJoinMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed CONFIRM_ROOM_JOIN\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::CREATE_ROOM_WITH_PASSWORD:
            {
                auto message = std::make_unique<CreateRoomWithPasswordMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed CREATE_ROOM_WITH_PASSWORD\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::CONFIRM_ROOM_CREATION:
            {
                auto message = std::make_unique<Message>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed CONFIRM_ROOM_CREATION\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::JOIN_OR_CREATE_ROOM:
            {
                auto message = std::make_unique<JoinRoomWithPasswordMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed JOIN_OR_CREATE_ROOM\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::JOIN_OR_CREATE_ROOM_RANDOM:
            {
                auto message = std::make_unique<Message>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed JOIN_OR_CREATE_ROOM_RANDOM\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::SET_EXTRA_INFO:
        case MessageType::SET_HOST_MIGRATION:
        case MessageType::SET_ROOM_NAME:
        case MessageType::SET_ROOM_PASSWORD:
        case MessageType::SET_MAX_PLAYERS:
        case MessageType::SET_ROOM_PRIVATE:
            {
                auto message = std::make_unique<RoomSettingMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed SET_ROOM_SETTING\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::PACKET_TO_CLIENT:
        case MessageType::PACKET_TO_CLIENTS:
        case MessageType::PACKET_TO_SERVER:
            {
                auto message = std::make_unique<PacketMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed PACKET\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::RPC_TO_CLIENT:
        case MessageType::RPC_TO_CLIENTS:
        case MessageType::RPC_TO_SERVER:
            {
                auto message = std::make_unique<RPCMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed RPC\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::CONFIRM_CONNECTION:
            {
                auto message = std::make_unique<ConfirmConnectionMessage>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed CONFIRM_CONNECTION\n" );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        case MessageType::ROOM_CREATE:
        case MessageType::ROOM_JOIN:
        case MessageType::ROOM_LEAVE:
        case MessageType::MESSAGE:
        case MessageType::ERROR:
        case MessageType::GET_ROOM_INFO:
        case MessageType::GET_ROOM_LIST:
        case MessageType::PLAYER_JOINED:
        case MessageType::PLAYER_LEFT:
        case MessageType::KICK_CLIENT:
        case MessageType::HOST_LEFT:
            {
                auto message = std::make_unique<Message>();
                serialize::ReadStream readStream(data, dataLength);
                if (!message->deserialize( readStream ) )
                {
                    printf( "error: serialize read failed. type: %s\n", MessageTypeToString(type).c_str() );
                    return std::make_unique<BaseMessage>();
                }
                return message;
            }
        default:
            {
                printf( "error: unknown message type: %s\n", MessageTypeToString(type).c_str() );
                return std::make_unique<BaseMessage>();
            }
    }
}

std::string MessageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::ERROR: return "ERROR";
        case MessageType::ROOM_CREATE: return "ROOM_CREATE";
        case MessageType::ROOM_JOIN: return "ROOM_JOIN";
        case MessageType::ROOM_LEAVE: return "ROOM_LEAVE";
        case MessageType::RPC_TO_SERVER: return "RPC_TO_SERVER";
        case MessageType::RPC_TO_CLIENTS: return "RPC_TO_CLIENTS";
        case MessageType::RPC_TO_CLIENT: return "RPC_TO_CLIENT";
        case MessageType::MESSAGE: return "MESSAGE";
        case MessageType::GET_ROOM_LIST: return "GET_ROOM_LIST";
        case MessageType::CONFIRM_ROOM_CREATION: return "CONFIRM_ROOM_CREATION";
        case MessageType::CONFIRM_ROOM_JOIN: return "CONFIRM_ROOM_JOIN";
        case MessageType::ROOM_PEER_JOIN: return "ROOM_PEER_JOIN";
        case MessageType::ROOM_PEER_LEFT: return "ROOM_PEER_LEFT";
        case MessageType::NOT_MESSAGE: return "NOT_MESSAGE";
        case MessageType::JOIN_ROOM_WITH_PASSWORD: return "JOIN_ROOM_WITH_PASSWORD";
        case MessageType::SET_ROOM_PASSWORD: return "SET_ROOM_PASSWORD";
        case MessageType::SET_MAX_PLAYERS: return "SET_MAX_PLAYERS";
        case MessageType::SET_ROOM_PRIVATE: return "SET_ROOM_PRIVATE";
        case MessageType::SET_HOST_MIGRATION: return "SET_HOST_MIGRATION";
        case MessageType::SET_ROOM_NAME: return "SET_ROOM_NAME";
        case MessageType::SET_EXTRA_INFO: return "SET_EXTRA_INFO";
        case MessageType::GET_ROOM_INFO: return "GET_ROOM_INFO";
        case MessageType::PACKET_TO_SERVER: return "PACKET_TO_SERVER";
        case MessageType::PACKET_TO_CLIENTS: return "PACKET_TO_CLIENTS";
        case MessageType::PACKET_TO_CLIENT: return "PACKET_TO_CLIENT";
        case MessageType::KICK_CLIENT: return "KICK_CLIENT";
        case MessageType::JOIN_OR_CREATE_ROOM: return "JOIN_OR_CREATE_ROOM";
        case MessageType::JOIN_OR_CREATE_ROOM_RANDOM: return "JOIN_OR_CREATE_ROOM_RANDOM";
        case MessageType::PLAYER_JOINED: return "PLAYER_JOINED";
        case MessageType::PLAYER_LEFT: return "PLAYER_LEFT";
        case MessageType::CREATE_ROOM_WITH_PASSWORD: return "CREATE_ROOM_WITH_PASSWORD";
        case MessageType::HOST_LEFT: return "HOST_LEFT";
        case MessageType::CONFIRM_CONNECTION: return "CONFIRM_CONNECTION";
        default: return "UNKNOWN";
    }
}

std::string EventTypeToString(ENetEventType type) {
    switch (type) {
        case ENET_EVENT_TYPE_NONE: return "NONE";
        case ENET_EVENT_TYPE_CONNECT: return "CONNECT";
        case ENET_EVENT_TYPE_DISCONNECT: return "DISCONNECT";
        case ENET_EVENT_TYPE_RECEIVE: return "RECEIVE";
        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: return "DISCONNECT_TIMEOUT";
        default: return "UNKNOWN";
    }
}