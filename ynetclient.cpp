#include "ynetclient.h"
#include "thirdparty/enet_compress.h"

YNetClient::YNetClient(uint32_t protocolVersion) : 
    client(nullptr), 
    peer(nullptr), 
    running(false),
    protocolVersion(protocolVersion) {}

YNetClient::~YNetClient() {
    disconnect();
}

void YNetClient::messageLoop() {
    while (running) {
        if (client == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ENetEvent event;
        int result = enet_host_service(client, &event, 2000);
        
        if (result > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    try {
                        auto baseMsg = Message::Deserialize(event.packet->data, event.packet->dataLength);
                        if (baseMsg->type == MessageType::NOT_MESSAGE) {
                            printf("error: serialize read failed\n");
                            exit(1);
                            return;
                        }

                        MessageType messageType = static_cast<MessageType>(baseMsg->type);
                        std::cout << "Received message type: " << MessageTypeToString(messageType) << std::endl;
                        std::cout << "Raw data received: ";
                        for (size_t i = 0; i < event.packet->dataLength; i++) {
                            std::cout << static_cast<int>(event.packet->data[i]) << " ";
                        }
                        std::cout << std::endl;
                        switch (messageType) {
                            case MessageType::CONFIRM_CONNECTION: {
                                auto msg = dynamic_cast<ConfirmConnectionMessage*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Connected to server" << " (Client ID: " << msg->clientId << ")" << std::endl;
                                }
                                break;
                            }
                            case MessageType::CREATE_ROOM_WITH_PASSWORD: {
                                auto msg = dynamic_cast<CreateRoomWithPasswordMessage*>(baseMsg.get());
                                if (msg) {
                                    currentRoom = msg->roomCode;
                                    std::cout << "Room created successfully: " << currentRoom << " (ID: " << currentRoomId << ")" << std::endl;
                                }
                                break;
                            }

                            case MessageType::CONFIRM_ROOM_CREATION: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    currentRoom = msg->getStringData();
                                    std::cout << "Room created successfully: " << currentRoom << " (ID: " << currentRoomId << ")" << std::endl;
                                }
                                break;
                            }

                            case MessageType::CONFIRM_ROOM_JOIN: {
                                auto msg = dynamic_cast<ConfirmRoomJoinMessage*>(baseMsg.get());
                                if (msg) {
                                    currentRoom = msg->roomCode;
                                    std::cout << "Joined room: " << currentRoom << " (ID: " << currentRoomId << ")" << std::endl;
                                    std::cout << "Room players: " << msg->jsonRoomPlayers << std::endl;
                                } else {
                                    std::cout << "error: serialize read failed for CONFIRM_ROOM_JOIN\n" << std::endl;
                                }
                                break;
                            }

                            case MessageType::PLAYER_JOINED: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Player joined: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::PLAYER_LEFT: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Player left: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::MESSAGE: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Message: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::ERROR: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cerr << "Error: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::GET_ROOM_INFO: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Room info: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::GET_ROOM_LIST: {
                                auto msg = dynamic_cast<Message*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Room list: " << msg->getStringData() << std::endl;
                                }
                                break;
                            }

                            case MessageType::RPC_TO_CLIENTS: {
                                auto msg = dynamic_cast<RPCMessage*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "RPC received (type: " << msg->rpcType << ")" << std::endl;
                                    // Handle RPC data as needed
                                }
                                break;
                            }

                            case MessageType::PACKET_TO_CLIENTS: {
                                auto msg = dynamic_cast<PacketMessage*>(baseMsg.get());
                                if (msg) {
                                    std::cout << "Packet received from server" << std::endl;
                                    // Handle packet data as needed
                                }
                                break;
                            }

                            default:
                                std::cout << "Unhandled message type: " << MessageTypeToString(messageType) << std::endl;
                                break;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error deserializing message: " << e.what() << std::endl;
                    }
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "Disconnected from server" << std::endl;
                    peer = nullptr;
                    currentRoom.clear();
                    currentRoomId = 0;
                    break;

                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "Connected to server" << std::endl;
                    break;

                default:
                    std::cout << "Unknown event type: " << EventTypeToString(event.type) << std::endl;
                    break;
            }
        } else if (result < 0) {
            std::cerr << "Error in enet_host_service: " << result << std::endl;
        }
    }
}

bool YNetClient::setupClient() {
    // Create a client client with 1 outgoing connection and 2 channels
    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);
    if (client == NULL) {
        std::cerr << "An error occurred while trying to create an ENet client host." << std::endl;
        return false;
    }

    // Enable range coder compression
    if (enet_host_compress_with_range_coder(client) < 0) {
        std::cerr << "Failed to enable range coder compression" << std::endl;
        enet_host_destroy(client);
        client = nullptr;
        return false;
    }

    std::cout << "Client created successfully" << std::endl;
    return true;
}

bool YNetClient::connect(const std::string& hostname, uint16_t port, uint32_t protocolVersion) {
    std::cout << "Attempting to connect to " << hostname << ":" << port << std::endl;

    if (!setupClient()) {
        std::cerr << "Failed to setup client" << std::endl;
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, hostname.c_str());
    address.port = port;
    
    std::cout << "Connecting to server..." << std::endl;
    peer = enet_host_connect(client, &address, 2, protocolVersion);
    if (peer == NULL) {
        std::cerr << "No available peers for initiating an ENet connection." << std::endl;
        enet_host_destroy(client);
        client = nullptr;
        return false;
    }

    // Start message processing thread
    running = true;
    // Wait for connection with multiple attempts
    std::cout << "Waiting for connection event..." << std::endl;

    ENetEvent event = {};
    int attempts = 0;
    const int MAX_ATTEMPTS = 5; // 3 seconds total (30 * 100ms)
    
    while (attempts < MAX_ATTEMPTS) {
        int serviceResult = enet_host_service(client, &event, 2000);
        if (serviceResult > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                std::cout << "Connected to server successfully!" << std::endl;
                messageThread = std::thread(&YNetClient::messageLoop, this);
                return true;
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Handle any initial messages
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                std::cerr << "Connection was disconnected by server" << std::endl;
                break;
            }
        } else if (serviceResult < 0) {
            std::cerr << "Error in enet_host_service: " << serviceResult << std::endl;
            break;
        }
        attempts++;
    }

    std::cerr << "Connection timeout after " << attempts << " attempts" << std::endl;
    enet_peer_reset(peer);
    peer = nullptr;
    enet_host_destroy(client);
    client = nullptr;
    running = false;
    if (messageThread.joinable()) {
        messageThread.join();
    }
    return false;
}

void YNetClient::disconnect() {
    running = false;
    if (messageThread.joinable()) {
        messageThread.join();
    }

    if (peer != nullptr) {
        enet_peer_disconnect(peer, 0);
        while (enet_host_service(client, nullptr, 1000) > 0) {
            // Wait for disconnect
        }
        peer = nullptr;
    }
    if (client != nullptr) {
        enet_host_destroy(client);
        client = nullptr;
    }
    currentRoom.clear();
}

bool YNetClient::sendMessage(BaseMessage& msg) {
    if (peer == nullptr) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }

    const size_t MAX_MESSAGE_SIZE = 4096;
    uint8_t buffer[MAX_MESSAGE_SIZE];

    serialize::WriteStream writeStream(buffer, sizeof(buffer));
    if (!msg.serialize( writeStream ))
    {
        printf( "error: serialize write failed\n" );
        exit( 1 );
    }
    writeStream.Flush();

    ENetPacket* packet = enet_packet_create(buffer, writeStream.GetBytesProcessed(), ENET_PACKET_FLAG_RELIABLE);
    return enet_peer_send(peer, 0, packet) == 0;
}


bool YNetClient::createRoom(const std::string& roomCode) {
    Message msg;
    msg.type = static_cast<uint8_t>(MessageType::ROOM_CREATE);
    msg.setStringData(roomCode);
    if (sendMessage(msg)) {
        std::cout << "Room creation request sent" << std::endl;
        // Don't set currentRoom yet - wait for server response with the actual room code
        return true;
    }
    std::cerr << "Failed to send room creation request" << std::endl;
    return false;
}

bool YNetClient::joinRoom(const std::string& roomCode, const std::string& password) {
    JoinRoomWithPasswordMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::JOIN_ROOM_WITH_PASSWORD);
    msg.roomCode = roomCode;
    msg.password = password;

    if (sendMessage(msg)) {
        std::cout << "Room join request sent" << std::endl;
        currentRoom = roomCode;
        return true;
    }
    std::cerr << "Failed to send room join request" << std::endl;
    return false;
}

bool YNetClient::leaveRoom() {
    if (currentRoom.empty()) {
        std::cout << "Not in any room" << std::endl;
        return false;
    }

    Message msg;
    msg.type = MessageType::ROOM_LEAVE;
    
    if (sendMessage(msg)) {
        std::cout << "Room leave request sent" << std::endl;
        currentRoom.clear();
        currentRoomId = 0;
        return true;
    }
    std::cerr << "Failed to send room leave request" << std::endl;
    return false;
}

bool YNetClient::sendRoomMessage(const std::string& message) {
    if (currentRoom.empty()) {
        std::cout << "Not in any room" << std::endl;
        return false;
    }
    
    Message msg;
    msg.type = MessageType::MESSAGE;
    msg.setStringData(message);
    
    if (sendMessage(msg)) {
        std::cout << "Message sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send message" << std::endl;
    return false;
}

bool YNetClient::createRoomWithPassword(const std::string& roomCode, const std::string& password) {
    CreateRoomWithPasswordMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::CREATE_ROOM_WITH_PASSWORD);
    msg.roomCode = roomCode;
    msg.password = password;
    
    if (sendMessage(msg)) {
        std::cout << "Room creation request with password sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room creation request with password" << std::endl;
    return false;
}

bool YNetClient::joinRoomWithPassword(const std::string& roomCode, const std::string& password) {
    JoinRoomWithPasswordMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::JOIN_ROOM_WITH_PASSWORD);
    msg.roomCode = roomCode;
    msg.password = password;

    if (sendMessage(msg)) {
        std::cout << "Room join request with password sent" << std::endl;
        currentRoom = roomCode;
        return true;
    }
    std::cerr << "Failed to send room join request with password" << std::endl;
    return false;
}

bool YNetClient::joinOrCreateRoom(const std::string& roomCode, const std::string& password) {
    JoinRoomWithPasswordMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::JOIN_OR_CREATE_ROOM);
    msg.roomCode = roomCode;
    msg.password = password;

    if (sendMessage(msg)) {
        std::cout << "Join or create room request sent" << std::endl;
        currentRoom = roomCode;
        return true;
    }
    std::cerr << "Failed to send join or create room request" << std::endl;
    return false;
}

bool YNetClient::joinOrCreateRoomRandom() {
    Message msg;
    msg.type = static_cast<uint8_t>(MessageType::JOIN_OR_CREATE_ROOM_RANDOM);
    
    if (sendMessage(msg)) {
        std::cout << "Join or create random room request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send join or create random room request" << std::endl;
    return false;
}

bool YNetClient::setRoomPassword(const std::string& password) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_ROOM_PASSWORD);
    msg.settingValue = password;

    if (sendMessage(msg)) {
        std::cout << "Room password update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room password update request" << std::endl;
    return false;
}

bool YNetClient::setMaxPlayers(int maxPlayers) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_MAX_PLAYERS);
    msg.settingValue = std::to_string(maxPlayers);
    
    if (sendMessage(msg)) {
        std::cout << "Max players update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send max players update request" << std::endl;
    return false;
}

bool YNetClient::setRoomPrivate(bool isPrivate) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_ROOM_PRIVATE);
    msg.settingValue = isPrivate ? "true" : "false";
    
    if (sendMessage(msg)) {
        std::cout << "Room private status update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room private status update request" << std::endl;
    return false;
}

bool YNetClient::setHostMigration(bool canHostMigrate) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_HOST_MIGRATION);
    msg.settingValue = canHostMigrate ? "true" : "false";
    
    if (sendMessage(msg)) {
        std::cout << "Host migration setting update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send host migration setting update request" << std::endl;
    return false;
}

bool YNetClient::setRoomName(const std::string& name) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_ROOM_NAME);
    msg.settingValue = name;
    
    if (sendMessage(msg)) {
        std::cout << "Room name update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room name update request" << std::endl;
    return false;
}

bool YNetClient::setExtraInfo(const std::string& info) {
    RoomSettingMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::SET_EXTRA_INFO);
    msg.settingValue = info;
    
    if (sendMessage(msg)) {
        std::cout << "Room extra info update request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room extra info update request" << std::endl;
    return false;
}

bool YNetClient::getRoomInfo() {
    Message msg;
    msg.type = static_cast<uint8_t>(MessageType::GET_ROOM_INFO);
    
    if (sendMessage(msg)) {
        std::cout << "Room info request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room info request" << std::endl;
    return false;
}

bool YNetClient::getRoomList() {
    Message msg;
    msg.type = static_cast<uint8_t>(MessageType::GET_ROOM_LIST);
    
    if (sendMessage(msg)) {
        std::cout << "Room list request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send room list request" << std::endl;
    return false;
}

bool YNetClient::kickClient(const std::string& clientId) {
    Message msg;
    msg.type = static_cast<uint8_t>(MessageType::KICK_CLIENT);
    msg.setStringData(clientId);
    
    if (sendMessage(msg)) {
        std::cout << "Kick client request sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send kick client request" << std::endl;
    return false;
}

bool YNetClient::sendRPCToServer(uint16_t rpcType, const std::vector<uint8_t>& data) {
    RPCMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::RPC_TO_SERVER);
    msg.rpcType = rpcType;
    msg.data = data;
    
    if (sendMessage(msg)) {
        std::cout << "RPC to server sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send RPC to server" << std::endl;
    return false;
}

bool YNetClient::sendRPCToClients(uint16_t rpcType, const std::vector<uint8_t>& data) {
    RPCMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENTS);
    msg.rpcType = rpcType;
    msg.data = data;
    
    if (sendMessage(msg)) {
        std::cout << "RPC to clients sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send RPC to clients" << std::endl;
    return false;
}

bool YNetClient::sendRPCToClient(const std::string& targetClientId, uint16_t rpcType, const std::vector<uint8_t>& data) {
    RPCMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENT);
    msg.rpcType = std::stoi(targetClientId);
    msg.data = data;
    
    if (sendMessage(msg)) {
        std::cout << "RPC to specific client sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send RPC to specific client" << std::endl;
    return false;
}

bool YNetClient::sendPacketToServer(const std::vector<uint8_t>& packetData) {
    PacketMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PACKET_TO_SERVER);
    msg.packetData = packetData;
    
    if (sendMessage(msg)) {
        std::cout << "Packet to server sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send packet to server" << std::endl;
    return false;
}

bool YNetClient::sendPacketToClients(const std::vector<uint8_t>& packetData) {
    PacketMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PACKET_TO_CLIENTS);
    msg.packetData = packetData;
    
    if (sendMessage(msg)) {
        std::cout << "Packet to clients sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send packet to clients" << std::endl;
    return false;
}

bool YNetClient::sendPacketToClient(const std::string& targetClientId, const std::vector<uint8_t>& packetData) {
    PacketMessage msg;
    msg.type = static_cast<uint8_t>(MessageType::PACKET_TO_CLIENT);
    msg.targetClientId = std::stoi(targetClientId);
    msg.packetData = packetData;
    
    if (sendMessage(msg)) {
        std::cout << "Packet to specific client sent" << std::endl;
        return true;
    }
    std::cerr << "Failed to send packet to specific client" << std::endl;
    return false;
}

void YNetClient::run() {
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  create_room <room_code> [password]" << std::endl;
    std::cout << "  join_room <room_code> [password]" << std::endl;
    std::cout << "  join_or_create <room_code> [password]" << std::endl;
    std::cout << "  join_random" << std::endl;
    std::cout << "  leave_room" << std::endl;
    std::cout << "  send_message <message>" << std::endl;
    std::cout << "  set_password <password>" << std::endl;
    std::cout << "  set_max_players <number>" << std::endl;
    std::cout << "  set_private <true|false>" << std::endl;
    std::cout << "  set_host_migration <true|false>" << std::endl;
    std::cout << "  set_room_name <name>" << std::endl;
    std::cout << "  set_extra_info <info>" << std::endl;
    std::cout << "  get_room_info" << std::endl;
    std::cout << "  get_room_list" << std::endl;
    std::cout << "  kick_client <client_id>" << std::endl;
    std::cout << "  quit" << std::endl;
    std::cout << "\nEnter command: ";

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "quit") { 
            break;
        } else if (cmd == "create_room") {
            std::string roomCode, password;
            iss >> roomCode;
            if (iss >> password) {
                createRoomWithPassword(roomCode, password);
            } else {
                createRoom(roomCode);
            }
        } else if (cmd == "join_room") {
            std::string roomCode, password;
            iss >> roomCode;
            if (iss >> password) {
                joinRoomWithPassword(roomCode, password);
            } else {
                joinRoom(roomCode);
            }
        } else if (cmd == "join_or_create") {
            std::string roomCode, password;
            iss >> roomCode;
            if (iss >> password) {
                joinOrCreateRoom(roomCode, password);
            } else {
                joinOrCreateRoom(roomCode);
            }
        } else if (cmd == "join_random") {
            joinOrCreateRoomRandom();
        } else if (cmd == "leave_room") {
            leaveRoom();
        } else if (cmd == "send_message") {
            std::string message;
            std::getline(iss, message);
            if (!message.empty() && message[0] == ' ') {
                message = message.substr(1);
            }
            sendRoomMessage(message);
        } else if (cmd == "set_password") {
            std::string password;
            iss >> password;
            setRoomPassword(password);
        } else if (cmd == "set_max_players") {
            int maxPlayers;
            iss >> maxPlayers;
            setMaxPlayers(maxPlayers);
        } else if (cmd == "set_private") {
            std::string value;
            iss >> value;
            setRoomPrivate(value == "true");
        } else if (cmd == "set_host_migration") {
            std::string value;
            iss >> value;
            setHostMigration(value == "true");
        } else if (cmd == "set_room_name") {
            std::string name;
            std::getline(iss, name);
            if (!name.empty() && name[0] == ' ') {
                name = name.substr(1);
            }
            setRoomName(name);
        } else if (cmd == "set_extra_info") {
            std::string info;
            std::getline(iss, info);
            if (!info.empty() && info[0] == ' ') {
                info = info.substr(1);
            }
            setExtraInfo(info);
        } else if (cmd == "get_room_info") {
            getRoomInfo();
        } else if (cmd == "get_room_list") {
            getRoomList();
        } else if (cmd == "kick_client") {
            std::string clientId;
            iss >> clientId;
            kickClient(clientId);
        } else {
            std::cout << "Unknown command" << std::endl;
        }

        std::cout << "\nEnter command: ";
    }
} 