#include "ynetserver.h"
#include "thirdparty/enet_compress.h"
#include <fstream>
#include <filesystem>

// Helper function to get current timestamp
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Helper function to write to error log
void writeErrorLog(const std::string& message) {
    std::ofstream logFile("error.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << getCurrentTimestamp() << "] " << message << std::endl;
        logFile.close();
    }
}

YNetServer::YNetServer(uint16_t port, uint16_t web_port) {
    std::cout << "Initializing server on port " << port << std::endl;
    
    // Initialize ENET
    if (enet_initialize() != 0) {
        throw std::runtime_error("Failed to initialize ENET");
    }
    std::cout << "ENET initialized successfully" << std::endl;

    // Create ENET server
    ENetAddress address = {};

    address.host = ENET_HOST_ANY;
    address.port = port;


    #define MAX_CLIENTS 4000

    std::cout << "Creating ENET server on port " << address.port << std::endl;
    std::cout << "Server will listen on all interfaces (ENET_HOST_ANY)" << std::endl;
    
    // Try to create the server with more specific parameters
    server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);

    if (server == nullptr) {
        std::cerr << "Failed to create ENET server. Error: " << strerror(errno) << std::endl;
        enet_deinitialize();
        throw std::runtime_error("Failed to create ENET server");
    }

    // Enable range coder compression
    if (enet_host_compress_with_range_coder(server) < 0) {
        std::cerr << "Failed to enable range coder compression" << std::endl;
        enet_host_destroy(server);
        enet_deinitialize();
        throw std::runtime_error("Failed to enable range coder compression");
    }

    std::cout << "ENET server created successfully" << std::endl;
    std::cout << "Server is now listening for connections on port " << port << std::endl;

    // Initialize Mongoose
    mg_mgr_init(&mgr);
    
    // Create web server
    std::string web_addr = "http://0.0.0.0:" + std::to_string(web_port);
    web_server = mg_http_listen(&mgr, web_addr.c_str(), handleWebRequest, this);
    if (web_server == nullptr) {
        enet_host_destroy(server);
        enet_deinitialize();
        throw std::runtime_error("Failed to create web server");
    }

    std::cout << "Server initialized on port " << port << std::endl;
    std::cout << "Web interface available at http://localhost:" << web_port << std::endl;
}

YNetServer::~YNetServer() {
    // Cleanup Mongoose
    mg_mgr_free(&mgr);
    
    // Cleanup ENET
    enet_host_destroy(server);
    enet_deinitialize();
}

void YNetServer::run() {
    ENetEvent event;
    std::cout << "Server event loop started" << std::endl;
    
    while (true) {
        // Handle ENET events with a small timeout
        int result = enet_host_service(server, &event, 0);
        if (result > 0) {
            if (debugging) {
                std::cout << "ENET event received, type: " << EventTypeToString(event.type) << std::endl;
            }
            Client* client = nullptr;
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                client = createClientForPeer(event.peer, event.data);
            } else {
                client = getClientForPeer(event.peer);
            }
            bool client_debugging = client->debugging;
            if (client != nullptr) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT: {
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &event.peer->address.host, ip_str, INET6_ADDRSTRLEN);
                        std::cout << "New connection from " 
                                << ip_str << ":" 
                                << event.peer->address.port 
                                << " (Peer ID: " << client->id << ")"
                                << " (Protocol: " << client->protocolVersion << ")" << std::endl;
                        
                        // Configure more forgiving timeout settings for this peer
                        enet_peer_timeout(event.peer,
                            16,    // timeout limit: 16 attempts before giving up
                            5000,  // timeout minimum: 5 seconds for first timeout
                            15000  // timeout maximum: 15 seconds for final timeout
                        );
                        enet_peer_ping_interval(event.peer, 1000);
                        
                        if (debugging) {
                            std::cout << "DEBUG: Configured peer timeout settings for client " << client->id << ":" << std::endl;
                            std::cout << "  Ping interval: " << event.peer->pingInterval << "ms" << std::endl;
                            std::cout << "  Ping timeout: " << event.peer->timeoutMinimum << "ms" << std::endl;
                            std::cout << "  Timeout limit: " << event.peer->timeoutLimit << " timeouts before disconnect" << std::endl;
                        }
                        
                        // Send the peer ID to the client
                        ConfirmConnectionMessage message;
                        message.type = MessageType::CONFIRM_CONNECTION;
                        message.clientId = client->id;
                        sendMessage(client, event.peer, message);
                        break;
                    }

                    case ENET_EVENT_TYPE_RECEIVE:
                        if (debugging || client_debugging) {
                            std::cout << "Received data from client " << client->id << std::endl;
                            std::cout << "First byte: " << event.packet->data[0] << "Data length: " << event.packet->dataLength << " bytes" << std::endl;
                            std::cout << "Channel: " << event.channelID << std::endl;
                        }
                        handleReceivedData(client, event.peer, event.packet->data, event.packet->dataLength);
                        enet_packet_destroy(event.packet);
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT:
                        std::cout << "Client disconnected (Client ID: " << client->id << ")" << std::endl;
                        if (client != nullptr) {
                            releaseClientId(client->id);
                            handleRoomLeave(client, event.peer);
                            clients.erase(event.peer);
                            delete client;
                        }
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                        std::cout << "Client disconnected (Client ID: " << client->id << ") from timeout." << std::endl;
                        if (client != nullptr) {
                            releaseClientId(client->id);
                            handleRoomLeave(client, event.peer);
                            clients.erase(event.peer);
                            delete client;
                        }
                        break;

                    default:
                        std::cout << "Unknown event type: " << event.type << std::endl;
                        break;
                }
            }
        } else if (result < 0) {
            std::cerr << "Error in enet_host_service: " << result << std::endl;

        }

        // Handle Mongoose events with a small timeout
        mg_mgr_poll(&mgr, 0);
    }
}

void YNetServer::handleReceivedData(Client* client, ENetPeer* peer, const uint8_t* data, size_t dataLength) {
    bool client_debugging = debugging;
    
    if (client != nullptr && client->debugging) {
        client_debugging = true;
    }

    if (client_debugging) {
        std::cout << "Raw data received: ";
        for (size_t i = 0; i < dataLength; i++) {
            std::cout << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::endl;
    }
    
    try {
        MessageType messageType = static_cast<MessageType>(data[0]);
        // if (messageType == MessageType::NOT_MESSAGE) {
        //     if (dataLength == 2 && data[1] == 255) {
        //         client->debugging = !client->debugging;
        //         std::cout << "DEBUG: Client toggled debugging: " << client->id << " Debugging: " << client->debugging
        //          << std::endl;
        //         return;
        //     }
        // }
        auto baseMsg = Message::Deserialize(data, dataLength);
        // Get client's protocol version (default to 0 if not set)
        uint32_t protocolVersion = client->protocolVersion;
        
        if (client_debugging) {
            std::cout << "DEBUG: Received message type: " << MessageTypeToString(messageType) 
                      << " from client " << client->id
                      << " and protocol version: " << protocolVersion << std::endl;
        }
        // Handle the message based on its type
        switch (messageType) {
            case MessageType::ROOM_CREATE:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRoomCreation(client, peer, message->getStringData(), "");
                }
                break;

            case MessageType::CREATE_ROOM_WITH_PASSWORD:
                {
                    auto message = dynamic_cast<CreateRoomWithPasswordMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRoomCreation(client, peer, message->roomCode, message->password);
                }
                break;
            case MessageType::ROOM_JOIN:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRoomJoin(client, peer, message->getStringData(), "");
                }
                break;
            case MessageType::JOIN_ROOM_WITH_PASSWORD:
                {
                    auto message = dynamic_cast<JoinRoomWithPasswordMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRoomJoin(client, peer, message->roomCode, message->password);
                }
                break;

            case MessageType::ROOM_LEAVE:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRoomLeave(client, peer);
                }
                break;

            case MessageType::MESSAGE:
                {
                    if (client_debugging) {
                        std::cout << "DEBUG: Handling MESSAGE" << std::endl;
                    }
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        broadcastMessage(client, peer, message->getStringData());
                }
                break;

            case MessageType::ERROR:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        std::cerr << "Received error message: " << message->getStringData() << std::endl;
                }
                break;

            case MessageType::RPC_TO_SERVER:
                {
                    auto message = dynamic_cast<RPCMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRPCToServer(client, peer, message->rpcType, message->data);
                }
                break;

            case MessageType::RPC_TO_CLIENTS:
                {
                    auto message = dynamic_cast<RPCMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRPCToClients(client, peer, message->rpcType, message->data);
                }
                break;

            case MessageType::RPC_TO_CLIENT:
                {
                    auto message = dynamic_cast<RPCMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleRPCToClient(client, peer, message->rpcType, message->data);
                }
                break;

            case MessageType::SET_ROOM_PASSWORD:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetRoomPassword(client, peer, message->settingValue);
                }
                break;

            case MessageType::SET_MAX_PLAYERS:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetMaxPlayers(client, peer, message->settingValue);
                }
                break;

            case MessageType::SET_ROOM_PRIVATE:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetRoomPrivate(client, peer, message->settingValue);
                }
                break;

            case MessageType::SET_HOST_MIGRATION:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetHostMigration(client, peer, message->settingValue);
                }
                break;

            case MessageType::SET_ROOM_NAME:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetRoomName(client, peer, message->settingValue);
                }
                break;

            case MessageType::SET_EXTRA_INFO:
                {
                    auto message = dynamic_cast<RoomSettingMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleSetExtraInfo(client, peer, message->settingValue);
                }
                break;

            case MessageType::GET_ROOM_INFO:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleGetRoomInfo(client, peer);
                }
                break;

            case MessageType::GET_ROOM_LIST:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleGetRoomList(client, peer, message->getStringData());
                }
                break;

            case MessageType::PACKET_TO_SERVER:
                {
                    auto message = dynamic_cast<PacketMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handlePacketToServer(client, peer, message->packetData, message->reliabilityFlags, message->channel);
                }
                break;

            case MessageType::PACKET_TO_CLIENTS:
                {
                    auto message = dynamic_cast<PacketMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handlePacketToClients(client, peer, message->packetData, message->reliabilityFlags, message->channel);
                }
                break;

            case MessageType::PACKET_TO_CLIENT:
                {
                    auto message = dynamic_cast<PacketMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handlePacketToClient(client, peer, message->targetClientId, message->packetData, message->reliabilityFlags, message->channel);
                }
                break;

            case MessageType::KICK_CLIENT:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE) {
                        std::cout << "DEBUG: Kicking client " << message->getStringData() << std::endl;
                        handleKickClient(client, peer, static_cast<uint16_t>(std::stoi(message->getStringData())));
                    }
                }
                break;

            case MessageType::JOIN_OR_CREATE_ROOM:
                {
                    auto message = dynamic_cast<JoinRoomWithPasswordMessage*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleJoinOrCreateRoom(client, peer, message->roomCode, message->password);
                }
                break;

            case MessageType::JOIN_OR_CREATE_ROOM_RANDOM:
                {
                    auto message = dynamic_cast<Message*>(baseMsg.get());
                    if (message && message->type != MessageType::NOT_MESSAGE)
                        handleJoinOrCreateRoomRandom(client, peer);
                }
                break;

            default:
                if (client_debugging) {
                    std::cout << "DEBUG: Unknown message type: " << static_cast<int>(messageType) << std::endl;
                }
                break;
        }
    } catch (const std::exception& e) {
        std::string errorMsg = "Received raw data that couldn't be deserialized as a Message object\nError: " + std::string(e.what());
        
        // Add raw data to error message
        errorMsg += "\nRaw data: ";
        for (size_t i = 0; i < dataLength; i++) {
            errorMsg += std::to_string(static_cast<int>(data[i]));
            errorMsg += " ";
        }
        errorMsg += "\nRaw data length: " + std::to_string(dataLength) + " bytes";
        
        std::cout << errorMsg << std::endl;
        writeErrorLog(errorMsg);
        
        // Echo back the raw data to confirm receipt
        ENetPacket* packet = enet_packet_create(data, dataLength, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
        enet_host_flush(server);
    }
}

uint16_t YNetServer::roomIdForPeer(ENetPeer* peer) {   
    Client* client = getClientForPeer(peer);
    if (client != nullptr) {
        return client->roomId;
    }
    return 0;
}

Client* YNetServer::getClientForPeer(ENetPeer* peer) {
    auto clientIt = clients.find(peer);
    if (clientIt != clients.end()) {
        return clientIt->second;
    }
    return nullptr;
}
Client* YNetServer::createClientForPeer(ENetPeer* peer, uint32_t protocolVersion) {
    Client* client = new Client();
    std::lock_guard<std::mutex> lock(dataMutex);
    client->id = generateClientId();
    client->currentRoom = {};
    client->protocolVersion = protocolVersion;
    client->debugging = false;
    clients[peer] = client;
    return client;
}

void YNetServer::sendMessage(Client* client, ENetPeer* peer, BaseMessage& msg, uint8_t reliabilityFlags, uint8_t channel) {
    if (debugging) {
        std::cout << "DEBUG: Sending message type: " << MessageTypeToString(msg.type) 
                << " reliability " << std::to_string(reliabilityFlags) << " channel " << std::to_string(channel) 
                  << " to client " << client->id << std::endl;
    }
    const uint32_t bufferSize = 65532;  // Changed to be a multiple of 4 (65532 = 16383 * 4)
    uint8_t buffer[bufferSize];
    serialize::WriteStream writeStream(buffer, bufferSize);
    if ( !msg.serialize( writeStream ) )
    {
        printf( "error: serialize write failed\n" );
        exit( 1 );
    }
    writeStream.Flush();
    ENetPacket* packet = enet_packet_create(buffer, writeStream.GetBytesProcessed(), reliabilityFlags);
    enet_peer_send(peer, channel, packet);
    enet_host_flush(server);
    if (debugging) {
        std::cout << "DEBUG: Message sent successfully. Raw bytes: ";
        for (int i = 0; i < writeStream.GetBytesProcessed(); i++) {
            std::cout << static_cast<int>(buffer[i]) << " ";
        }
        std::cout << std::endl;
    }
}

void YNetServer::sendRPCMessage(Client* client, ENetPeer* peer, RPCMessage& msg) {
    if (debugging) {
        std::cout << "DEBUG: Sending message type: " << MessageTypeToString(msg.type) 
                  << " to peer " << reinterpret_cast<uintptr_t>(peer) << std::endl;
    }
    
    uint8_t buffer[sizeof(msg)];
    serialize::WriteStream writeStream(buffer, sizeof(buffer) );
    if ( !msg.serialize( writeStream ) )
    {
        printf( "error: serialize write failed\n" );
        exit( 1 );
    }
    writeStream.Flush();
    ENetPacket* packet = enet_packet_create(buffer, writeStream.GetBytesProcessed(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(server);
    if (debugging) {
        std::cout << "DEBUG: Message sent successfully" << std::endl;
    }
}


bool YNetServer::isValidRoomCode(const std::string& code) {
    if (code.length() != 6) {
        return false;
    }
    for (char c : code) {
        if (std::find(ROOM_CODE_CHARS.begin(), ROOM_CODE_CHARS.end(), c) == ROOM_CODE_CHARS.end()) {
            return false;
        }
    }
    return true;
}

std::string YNetServer::generateRoomCode() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis_full(0, ROOM_CODE_CHARS.size() - 1);
    static std::uniform_int_distribution<> dis_sq(0, ROOM_CODE_CHARS_SQ.size() - 1);
    
    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i) {
        if (i == 1 || i == 3) {
            code += ROOM_CODE_CHARS_SQ[dis_sq(gen)];  // Use the smaller distribution
        } else {
            code += ROOM_CODE_CHARS[dis_full(gen)];   // Use the larger distribution
        }
    }
    return code;
}

uint16_t YNetServer::generateRoomId() {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    // Try to find an unused ID
    while (usedRoomIds.find(nextRoomId) != usedRoomIds.end()) {
        nextRoomId = (nextRoomId + 1) % (MAX_ROOM_ID + 1);
    }
    
    uint16_t id = nextRoomId;
    usedRoomIds.insert(id);
    nextRoomId = (nextRoomId + 1) % (MAX_ROOM_ID + 1);
    return id;
}

void YNetServer::releaseRoomId(uint16_t id) {
    std::lock_guard<std::mutex> lock(dataMutex);
    usedRoomIds.erase(id);
}


uint16_t YNetServer::generateClientId() {
    const uint16_t MIN_CLIENT_ID = 1000;
    
    // Initialize nextClientId if it hasn't been set yet
    if (nextClientId < MIN_CLIENT_ID) {
        nextClientId = MIN_CLIENT_ID;
    }
    
    // Keep track of where we started to detect if we've tried all IDs
    uint16_t startId = nextClientId;
    bool triedAllIds = false;
    
    // Try to find an unused ID
    while (usedClientIds.find(nextClientId) != usedClientIds.end()) {
        nextClientId = (nextClientId + 1 - MIN_CLIENT_ID) % (MAX_CLIENT_ID - MIN_CLIENT_ID + 1) + MIN_CLIENT_ID;
        
        // If we've come back to where we started, we've tried all IDs
        if (nextClientId == startId) {
            triedAllIds = true;
            break;
        }
    }
    
    // If we've tried all IDs and still haven't found an unused one, throw an exception
    if (triedAllIds) {
        throw std::runtime_error("No available client IDs");
    }
    
    uint16_t id = nextClientId;
    usedClientIds.insert(id);
    nextClientId = (nextClientId + 1 - MIN_CLIENT_ID) % (MAX_CLIENT_ID - MIN_CLIENT_ID + 1) + MIN_CLIENT_ID;
    return id;
}


void YNetServer::releaseClientId(uint16_t id) {
    std::lock_guard<std::mutex> lock(dataMutex);
    usedClientIds.erase(id);
}
void YNetServer::handleRoomCreation(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password) {
    if (debugging) {
        std::cout << "DEBUG: Attempting to create room with code: " << roomCode << " " << password
                  << " and protocol version: " << client->protocolVersion << std::endl;
    }

    // Generate a random room code if the client sent an empty one
    std::string finalRoomCode = roomCode;
    if (finalRoomCode.empty()) {
        finalRoomCode = generateRoomCode();
        if (debugging) {
            std::cout << "DEBUG: Generated random room code: " << finalRoomCode << std::endl;
        }
    }

    if (!isValidRoomCode(finalRoomCode)) {
        if (debugging) {
            std::cout << "DEBUG: Room creation failed - Invalid room code format" << std::endl;
        }
        Message error;
        error.type = static_cast<uint8_t>(MessageType::ERROR);
        error.setStringData("Invalid room code format");
        sendMessage(client, peer, error);
        return;
    }

    if (rooms.find(finalRoomCode) != rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room creation failed - Room already exists" << std::endl;
        }
        Message error;
        error.type = static_cast<uint8_t>(MessageType::ERROR);
        error.setStringData("Room already exists");
        sendMessage(client, peer, error);
        return;
    }

    // Generate a unique room ID
    uint16_t roomId = generateRoomId();

    Room room;
    room.roomId = roomId;
    room.roomCode = finalRoomCode;
    room.protocol = client->protocolVersion;
    room.host = peer;
    room.maxPlayers = 4;
    room.isPrivate = false;
    room.canHostMigrate = false;
    room.extraInfo = "";
    room.roomName = "";
    room.password = password;
    room.hashed_password = password.empty() ? 0 : stringToHash(password);

    client->currentRoom = finalRoomCode;
    client->roomId = roomId;

    // Add client to room's client list
    room.clients[peer] = client->id;

    // Add room to rooms map
    rooms[finalRoomCode] = room;

    if (debugging) {
        std::cout << "DEBUG: Room created with " << room.clients.size() << " clients" << std::endl;
    }

    Message confirmRoomCreation;
    confirmRoomCreation.type = static_cast<uint8_t>(MessageType::CONFIRM_ROOM_CREATION);
    confirmRoomCreation.setStringData(finalRoomCode);
    sendMessage(client, peer, confirmRoomCreation);

    std::cout << "Room " << finalRoomCode << " (ID: " << roomId << ") created by " << client->id << std::endl;
}

void YNetServer::handleRoomJoin(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password) {
    if (debugging) {
        std::cout << "DEBUG: Attempting to join room: " << roomCode << std::endl;
    }

    // If room code is empty, try to find a random matching room
    if (roomCode.empty()) {
        std::vector<std::string> matchingRooms;
        
        // Find all rooms that match our criteria
        for (const auto& [code, room] : rooms) {
            if (!room.isPrivate && 
                (room.password.empty() || room.password == password) && 
                room.protocol == client->protocolVersion && 
                room.clients.size() < static_cast<size_t>(room.maxPlayers)) {
                matchingRooms.push_back(code);
            }
        }

        if (matchingRooms.empty()) {
            if (debugging) {
                std::cout << "DEBUG: No matching rooms found for random join" << std::endl;
            }
            Message error;
            error.type = static_cast<uint8_t>(MessageType::ERROR);
            error.setStringData("No available rooms found");
            sendMessage(client, peer, error);
            return;
        }

        // Randomly select a room from matching rooms
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, matchingRooms.size() - 1);
        std::string selectedRoom = matchingRooms[dis(gen)];

        if (debugging) {
            std::cout << "DEBUG: Selected random room: " << selectedRoom << std::endl;
        }

        // Continue with the join process using the selected room
        auto roomIt = rooms.find(selectedRoom);
        if (roomIt == rooms.end()) {
            Message error;
            error.setMessageType(MessageType::ERROR);
            error.setStringData("Selected room no longer available");
            sendMessage(client, peer, error);
            return;
        }

        Room& room = roomIt->second;
        client->roomId = room.roomId;
        client->currentRoom = selectedRoom;
        room.clients[peer] = client->id;

        // Send list of all players in the room to the new player
        std::vector<uint16_t> playerIds;
        playerIds.push_back(room.clients[room.host]); // Add host first
        for (const auto& [otherPeer, clientId] : room.clients) {
            if (otherPeer != room.host) {
                playerIds.push_back(clientId);
            }
        }

        // Send room players list
        ConfirmRoomJoinMessage confirmRoomJoin;
        confirmRoomJoin.type = static_cast<uint8_t>(MessageType::CONFIRM_ROOM_JOIN);
        confirmRoomJoin.roomCode = selectedRoom;
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < playerIds.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << playerIds[i] << "\"";
        }
        ss << "]";
        confirmRoomJoin.jsonRoomPlayers = ss.str();
        sendMessage(client, peer, confirmRoomJoin);

        // Notify other clients about the new player
        Message notification;
        notification.setMessageType(MessageType::PLAYER_JOINED);
        notification.setStringData(std::to_string(client->id));
        for (const auto& [otherPeer, _] : room.clients) {
            if (otherPeer != peer) {
                Client* otherClient = getClientForPeer(otherPeer);
                if (otherClient != nullptr)
                    sendMessage(otherClient, otherPeer, notification);
            }
        }

        std::cout << "Client " << clients[peer]->id << " joined random room " << selectedRoom << " (ID: " << room.roomId << ")" << std::endl;
        return;
    }

    // Original room join logic for non-empty room codes
    auto roomIt = rooms.find(roomCode);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room join failed - Room not found" << std::endl;
        }
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Room not found");
        sendMessage(client, peer, error);
        return;
    }

    Room& room = roomIt->second;
    if (room.protocol != client->protocolVersion) {
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Protocol version mismatch");
        sendMessage(client, peer, error);
        return;
    }

    if (room.password != password) {
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Invalid password");
        sendMessage(client, peer, error);
        return;
    }

    if (room.isPrivate) {
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Room is private");
        sendMessage(client, peer, error);
        return;
    }

    if (room.clients.size() >= static_cast<size_t>(room.maxPlayers)) {
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Room is full");
        sendMessage(client, peer, error);
        return;
    }

    client->roomId = room.roomId;
    client->currentRoom = roomCode;
    room.clients[peer] = client->id;
    
    ConfirmRoomJoinMessage response;
    response.type = static_cast<uint8_t>(MessageType::CONFIRM_ROOM_JOIN);
    response.roomCode = roomCode;

    // Send list of all players in the room to the new player
    std::vector<uint16_t> playerIds;
    playerIds.push_back(room.clients[room.host]); // Add host first
    for (const auto& [otherPeer, clientId] : room.clients) {
        if (otherPeer != room.host) {
            playerIds.push_back(clientId);
        }
    }

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < playerIds.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << playerIds[i] << "\"";
    }
    ss << "]";
    response.jsonRoomPlayers = ss.str();
    sendMessage(client, peer, response);

    // Notify other clients about the new player
    Message notification;
    notification.setMessageType(MessageType::PLAYER_JOINED);
    notification.setStringData(std::to_string(client->id));
    for (const auto& [otherPeer, _] : room.clients) {
        if (otherPeer != peer) {
            Client* otherClient = getClientForPeer(otherPeer);
            if (otherClient != nullptr)
                sendMessage(otherClient, otherPeer, notification);
        }
    }

    std::cout << "Client " << client->id << " joined room " << roomCode << " (ID: " << room.roomId << ")" << std::endl;
}

void YNetServer::handleRoomLeave(Client* client, ENetPeer* peer) {
    const std::string& roomCode = client->currentRoom;
    auto roomIt = rooms.find(roomCode);
    if (roomIt != rooms.end()) {
        Room& room = roomIt->second;
        room.clients.erase(peer);

        // If host left, assign new host if room canHostMigrate or close room
        if (room.host == peer) {
            if (room.clients.empty()) {
                // Release the room ID before erasing the room
                releaseRoomId(room.roomId);
                rooms.erase(roomIt);
                roomsByID.erase(room.roomId);
                std::cout << "Room " << roomCode << " (ID: " << room.roomId << ") removed (empty)" << std::endl;
            }
            else if (!room.canHostMigrate) {
                for (const auto& [otherPeer, clientId] : room.clients) {
                    Client* otherClient = getClientForPeer(otherPeer);
                    if (otherClient != nullptr) {
                        {
                            Message notification;
                            notification.setMessageType(MessageType::PLAYER_LEFT);
                            notification.setStringData(std::to_string(client->id));
                            sendMessage(otherClient, otherPeer, notification);
                        }
                        {
                            Message notification;
                            notification.setMessageType(MessageType::HOST_LEFT);
                            notification.setStringData(std::to_string(client->id));
                            sendMessage(otherClient, otherPeer, notification);
                        }
                        {
                            Message notification;
                            notification.setMessageType(MessageType::ERROR);
                            notification.setStringData("The host has left the room.");
                            sendMessage(otherClient, otherPeer, notification);
                        }
                        otherClient->currentRoom = "";
                        otherClient->roomId = 0;
                        enet_peer_disconnect_later(otherPeer, 0);
                    }
                }
                releaseRoomId(room.roomId);
                rooms.erase(roomIt);
                roomsByID.erase(room.roomId);
                std::cout << "Room " << roomCode << " (ID: " << room.roomId << ") removed (empty)" << std::endl;
            }
             else {
                room.host = room.clients.begin()->first;
                Client* newHost = getClientForPeer(room.host);
                if (newHost != nullptr) {
                    Message notification;
                    notification.setMessageType(MessageType::MESSAGE);
                    notification.setStringData("New host assigned");
                    sendMessage(newHost, room.host, notification);
                } else {
                    std::cout << "New host is not found" << std::endl;
                    // TODO: Remove the room
                    releaseRoomId(room.roomId);
                    rooms.erase(roomIt);
                    roomsByID.erase(room.roomId);
                    std::cout << "Room " << roomCode << " (ID: " << room.roomId << ") removed (empty)" << std::endl;
                }
            }
        } else {
            // Notify other clients
            Message notification;
            notification.setMessageType(MessageType::PLAYER_LEFT);
            notification.setStringData(std::to_string(client->id));
            for (const auto& [otherPeer, _] : room.clients) {
                Client* otherClient = getClientForPeer(otherPeer);
                if (otherClient != nullptr)
                    sendMessage(otherClient, otherPeer, notification);
            }
        }
    }
    client->currentRoom = {};
    client->roomId = 0;
    std::cout << "Client " << client->id << " left room " << roomCode << std::endl;
}

void YNetServer::handleWebRequest(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        if (mg_strcmp(hm->uri, mg_str("/")) == 0) {
            // Get the server instance from the connection's user data
            YNetServer* server = static_cast<YNetServer*>(c->fn_data);
            if (server) {
                std::string statusPage = server->generateStatusPage();
                mg_http_reply(c, 200, "Content-Type: text/html\r\n", "%s", statusPage.c_str());
            } else {
                mg_http_reply(c, 500, "Content-Type: text/plain\r\n", "Internal Server Error");
            }
        }
    }
}

std::string YNetServer::generateStatusPage() {
    std::lock_guard<std::mutex> lock(dataMutex);  // Lock the mutex while accessing shared data
    
    std::stringstream ss;
    ss << "<!DOCTYPE html>\n"
       << "<html>\n"
       << "<head>\n"
       << "    <title>YNet Server Status</title>\n"
       << "    <style>\n"
       << "        body { font-family: Arial, sans-serif; margin: 20px; }\n"
       << "        .room { border: 1px solid #ccc; padding: 10px; margin: 10px 0; }\n"
       << "        .player { color: #666; }\n"
       << "    </style>\n"
       << "</head>\n"
       << "<body>\n"
       << "    <h1>YNet Server Status</h1>\n"
       << "    <p>Total Rooms: " << rooms.size() << "</p>\n"
       << "    <p>Total Players: " << clients.size() << "</p>\n"
       << "    <h2>Active Rooms</h2>\n";

    for (const auto& [roomCode, room] : rooms) {
        ss << "    <div class='room'>\n"
           << "        <h3>Room: " << roomCode << "</h3>\n"
           << "        <p>Protocol: " << room.protocol << "</p>\n"
           << "        <p>Players: " << room.clients.size() << "/" << room.maxPlayers << "</p>\n"
           << "        <p>Status: " << (room.isPrivate ? "Private" : "Public") << "</p>\n"
           << "        <div class='players'>\n";
        
        for (const auto& [peer, clientId] : room.clients) {
            ss << "            <p class='player'>Player: " << clientId << "</p>\n";
        }
        
        ss << "        </div>\n"
           << "    </div>\n";
    }

    ss << "    <script>\n"
       << "        setTimeout(function() { location.reload(); }, 5000);\n"
       << "    </script>\n"
       << "</body>\n"
       << "</html>";
    
    return ss.str();
}

void YNetServer::handleRPC(Client* client, ENetPeer* peer, const uint16_t& rpcType, const std::vector<uint8_t>& data) {
    if (debugging) {
        std::cout << "DEBUG: Handling RPC in room: " << client->currentRoom
                  << " with type: " << rpcType << std::endl;
    }

    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: RPC failed - Room not found" << std::endl;
        }
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Room not found");
        sendMessage(client, peer, error);
        return;
    }

    Room& room = roomIt->second;
    if (room.protocol != client->protocolVersion) {
        Message error;
        error.setMessageType(MessageType::ERROR);
        error.setStringData("Protocol mismatch");
        sendMessage(client, peer, error);
        return;
    }

    RPCMessage rpc;
    rpc.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENTS);
    rpc.rpcType = rpcType;
    rpc.data = data;

    for (const auto& [otherPeer, _] : room.clients) {
        if (otherPeer != peer) {
            Client* otherClient = getClientForPeer(otherPeer);
            if (otherClient != nullptr)
                sendRPCMessage(otherClient, otherPeer, rpc);
        }
    }
}

RoomFilters RoomFilters::fromJson(const std::string& jsonStr) {
    RoomFilters filters;
    if (jsonStr.empty()) {
        return filters;
    }
    try {
            nlohmann::json j = nlohmann::json::parse(jsonStr);                
            if (j.contains("minPlayers"))
                filters.minPlayers = j["minPlayers"];
            if (j.contains("maxPlayers"))
                filters.maxPlayers = j["maxPlayers"];
            if (j.contains("nameContains"))
                filters.nameContains = j["nameContains"];
            if (j.contains("extraInfoContains"))
                filters.extraInfoContains = j["extraInfoContains"];
            if (j.contains("pageSize"))
                filters.pageSize = j["pageSize"];
            if (j.contains("pageNumber"))
                filters.pageNumber = j["pageNumber"];
    }
    catch (const std::exception& e) {
        // Handle parsing errors
        std::cerr << "Error parsing filters: " << e.what() << std::endl;
    }
    return filters;
}

void YNetServer::handleGetRoomList(Client* client, ENetPeer* peer, const std::string& filtersJson) {
    if (debugging) {
        std::cout << "DEBUG: Handling room list request for protocol: " << client->protocolVersion << std::endl;
        std::cout << "DEBUG: Current rooms count: " << rooms.size() << std::endl;
    }

    RoomFilters filters = RoomFilters::fromJson(filtersJson);
    
    Message response;
    response.setMessageType(MessageType::GET_ROOM_LIST);

    std::vector<const Room*> matchingRooms;
    for (const auto& [roomCode, room] : rooms) {
        if (debugging) {
            std::cout << "DEBUG: Checking room: " << roomCode << " with protocol: " << room.protocol << std::endl;
        }
        // Apply all filters
        if (room.protocol != client->protocolVersion) continue;
        
        int currentPlayers = room.clients.size();
        if (currentPlayers < filters.minPlayers) continue;
        if (currentPlayers > filters.maxPlayers) continue;
        if (!filters.nameContains.empty() && 
            room.roomName.find(filters.nameContains) == std::string::npos) continue;
        if (!filters.extraInfoContains.empty() && 
            room.extraInfo.find(filters.extraInfoContains) == std::string::npos) continue;
        matchingRooms.push_back(&room);
    }
    // Calculate pagination
    int totalRooms = matchingRooms.size();
    int startIndex = (filters.pageNumber - 1) * filters.pageSize;
    int endIndex = std::min(startIndex + filters.pageSize, totalRooms);
    // Build response JSON
    std::stringstream ss;
    ss << "{\"total\":" << totalRooms << ",\"rooms\":[";
    
    bool first = true;
    for (int i = startIndex; i < endIndex; i++) {
        const Room* room = matchingRooms[i];
        
        if (!first) ss << ",";
        ss << "{\"code\":\"" << room->roomCode << "\","
           << "\"players\":" << room->clients.size() << ","
           << "\"maxPlayers\":" << room->maxPlayers;
        if (!room->roomName.empty()) {
            ss << ",\"name\":\"" << room->roomName << "\"";
        }
        if (!room->password.empty()) {
            ss << ",\"pwd\":\"" << room->hashed_password << "\"";
        }
        if (room->isPrivate) {
            ss << ",\"isPrivate\":true";
        }
        if (!room->extraInfo.empty()) {
            ss << ",\"extra\":\"" << room->extraInfo << "\"";
        }
        ss << "}";
        first = false;
    }
    ss << "]}";
    response.setStringData(ss.str());
    if (debugging) {
        std::cout << "DEBUG: Sending room list response: " << response.getStringData() << std::endl;
    }

    sendMessage(client, peer, response);
}

void YNetServer::broadcastMessage(Client* client, ENetPeer* broadcaster_peer, const std::string& message) {
    if (debugging) {
        std::cout << "DEBUG: Broadcasting message to room: " << client->currentRoom 
                  << " with protocol: " << client->protocolVersion 
                  << " message: " << message << std::endl;
    }

    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for broadcast: " << client->currentRoom << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.protocol != client->protocolVersion) {
        if (debugging) {
            std::cout << "DEBUG: Protocol mismatch for broadcast" << std::endl;
        }
        return;
    }

    Message broadcast;
    broadcast.setMessageType(MessageType::MESSAGE);
    broadcast.setStringData(message);

    if (debugging) {
        std::cout << "DEBUG: Sending broadcast to " << room.clients.size() << " clients" << std::endl;
    }

    for (const auto& [peer, _] : room.clients) {
        if (peer == broadcaster_peer) {
            continue;
        }
        if (debugging) {
            std::cout << "DEBUG: Sending broadcast to peer: " << reinterpret_cast<uintptr_t>(peer) << std::endl;
        }
        sendMessage(client, peer, broadcast);
    }
}

uint32_t YNetServer::stringToHash(const std::string& str) {
        /* simple djb2 hashing */
        std::u32string u32str;
        for (char c : str) {
            u32str.push_back(static_cast<char32_t>(static_cast<unsigned char>(c)));
        }
    
        const char32_t *chr = u32str.c_str();
        uint32_t hashv = 5381;
        uint32_t c = *chr++;

        while (c) {
            hashv = (((hashv) << 5) + hashv) + c; /* hash * 33 + c */
            c = *chr++;
        }

        hashv = hash_fmix32(hashv);
        hashv = hashv & 0x7FFFFFFF; // Make it compatible with unsigned, since negative ID is used for exclusion
        return hashv;
}

void YNetServer::handleRPCToServer(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data) {
    if (debugging) {
        std::cout << "DEBUG: Handling RPC to server from client " << client->id << std::endl;
    }

    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for RPC to server" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: No host found for room" << std::endl;
        }
        return;
    }

    // Forward RPC to host
    RPCMessage rpc;
    rpc.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENTS);
    rpc.rpcType = rpcType;
    rpc.data = data;

    Client* hostClient = getClientForPeer(room.host);
    if (hostClient != nullptr) {
        sendRPCMessage(hostClient, room.host, rpc);
    }
}

void YNetServer::handleRPCToClients(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data) {
    if (debugging) {
        std::cout << "DEBUG: Handling RPC to clients from client " << client->id << std::endl;
    }

    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for RPC to clients" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Non-host peer attempting to send RPC to clients" << std::endl;
        }
        return;
    }

    // Forward RPC to all clients except sender
    RPCMessage rpc;
    rpc.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENTS);
    rpc.rpcType = rpcType;
    rpc.data = data;

    for (const auto& [otherPeer, _] : room.clients) {
        if (otherPeer != peer) {
            Client* otherClient = getClientForPeer(otherPeer);
            if (otherClient != nullptr) {
                sendRPCMessage(otherClient, otherPeer, rpc);
            }
        }
    }
}

void YNetServer::handleRPCToClient(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data) {
    if (debugging) {
        std::cout << "DEBUG: Handling RPC to specific client from client " << client->id << std::endl;
    }

    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for RPC to client" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Non-host peer attempting to send RPC to specific client" << std::endl;
        }
        return;
    }

    // Find target client
    ENetPeer* targetPeer = nullptr;
    for (const auto& [otherPeer, clientId] : room.clients) {
        if (clientId == (rpcType)) { // Using rpcType as target client ID
            targetPeer = otherPeer;
            break;
        }
    }

    if (targetPeer == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: Target client not found in room" << std::endl;
        }
        return;
    }

    // Forward RPC to target client
    RPCMessage rpc;
    rpc.type = static_cast<uint8_t>(MessageType::RPC_TO_CLIENTS);
    rpc.rpcType = rpcType;
    rpc.data = data;

    Client* targetClient = getClientForPeer(targetPeer);
    if (targetClient != nullptr) {
        sendRPCMessage(targetClient, targetPeer, rpc);
    }
}

void YNetServer::handleSetRoomPassword(Client* client, ENetPeer* peer, const std::string& newPassword) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set password" << std::endl;
        }
        return;
    }

    roomIt->second.password = newPassword;
    roomIt->second.hashed_password = newPassword.empty() ? 0 : stringToHash(newPassword);
    if (debugging) {
        std::cout << "DEBUG: Room password updated" << std::endl;
    }
}

void YNetServer::handleSetMaxPlayers(Client* client, ENetPeer* peer, const std::string& newMaxPlayers) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set max players" << std::endl;
        }
        return;
    }

    try {
        int maxPlayers = std::stoi(newMaxPlayers);
        if (maxPlayers > 0) {
            roomIt->second.maxPlayers = maxPlayers;
            if (debugging) {
                std::cout << "DEBUG: Room max players updated to " << maxPlayers << std::endl;
            }
        }
    } catch (const std::exception& e) {
        if (debugging) {
            std::cout << "DEBUG: Invalid max players value: " << e.what() << std::endl;
        }
    }
}

void YNetServer::handleSetRoomPrivate(Client* client, ENetPeer* peer, const std::string& newPrivate) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set private status" << std::endl;
        }
        return;
    }

    roomIt->second.isPrivate = (newPrivate == "true");
    if (debugging) {
        std::cout << "DEBUG: Room private status updated to " << roomIt->second.isPrivate << std::endl;
    }
}

void YNetServer::handleSetHostMigration(Client* client, ENetPeer* peer, const std::string& newCanHostMigrate) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set host migration" << std::endl;
        }
        return;
    }

    roomIt->second.canHostMigrate = (newCanHostMigrate == "true");
    if (debugging) {
        std::cout << "DEBUG: Room host migration updated to " << roomIt->second.canHostMigrate << std::endl;
    }
}

void YNetServer::handleSetRoomName(Client* client, ENetPeer* peer, const std::string& newRoomName) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set room name" << std::endl;
        }
        return;
    }

    roomIt->second.roomName = newRoomName;
    if (debugging) {
        std::cout << "DEBUG: Room name updated to " << newRoomName << std::endl;
    }
}

void YNetServer::handleSetExtraInfo(Client* client, ENetPeer* peer, const std::string& newExtraInfo) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end() || roomIt->second.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Invalid room or non-host attempting to set extra info" << std::endl;
        }
        return;
    }

    roomIt->second.extraInfo = newExtraInfo;
    if (debugging) {
        std::cout << "DEBUG: Room extra info updated" << std::endl;
    }
}

void YNetServer::handleGetRoomInfo(Client* client, ENetPeer* peer) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for get room info" << std::endl;
        }
        return;
    }

    // Create a copy of the room info without sensitive data
    auto& room = roomIt->second;
    // Convert room info to JSON
    std::stringstream ss;
    ss << "{"
       << "\"roomId\":" << room.roomId << ","
       << "\"roomCode\":\"" << room.roomCode << "\","
       << "\"roomName\":\"" << room.roomName << "\","
       << "\"protocol\":" << room.protocol << ","
       << "\"maxPlayers\":" << room.maxPlayers << ","
       << "\"clientCount\":" << room.clients.size();

        if (room.hashed_password != 0) {
            ss << ",\"pwd\":\"" << room.hashed_password << "\"";
        }
        if (room.isPrivate) {
            ss << ",\"isPrivate\":true";
        }
        if (room.canHostMigrate) {
            ss << ",\"canHostMigrate\":true";
        }
        if (!room.extraInfo.empty()) {
            ss << ",\"extraInfo\":\"" << room.extraInfo << "\"";
        }
        ss << "}";

    Message response;
    response.type = static_cast<uint8_t>(MessageType::GET_ROOM_INFO);
    response.setStringData(ss.str());
    sendMessage(client, peer, response);
}

void YNetServer::handlePacketToServer(Client* client, ENetPeer* peer, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for packet to server" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: No host found for room" << std::endl;
        }
        return;
    }

    // Verify client is in the room
    if (room.clients.find(peer) == room.clients.end()) {
        if (debugging) {
            std::cout << "DEBUG: Client not in room attempting to send packet to server" << std::endl;
        }
        return;
    }

    // Forward packet to host
    PacketMessage packet;
    packet.type = static_cast<uint8_t>(MessageType::PACKET_TO_SERVER);
    packet.targetClientId = client->id;
    packet.packetData = packetData;
    packet.reliabilityFlags = reliabilityFlags;
    packet.channel = channel;
    Client* hostClient = getClientForPeer(room.host);
    if (hostClient != nullptr) {
        sendMessage(hostClient, room.host, packet, reliabilityFlags, channel);
    }
}

void YNetServer::handlePacketToClients(Client* client, ENetPeer* peer, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for packet to clients" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Non-host peer attempting to send packet to clients" << std::endl;
        }
        return;
    }

    // Forward packet to all clients except sender
    PacketMessage packet;
    packet.type = static_cast<uint8_t>(MessageType::PACKET_TO_CLIENTS);
    packet.targetClientId = client->id;
    packet.packetData = packetData;
    packet.reliabilityFlags = reliabilityFlags;
    packet.channel = channel;
    for (const auto& [otherPeer, _] : room.clients) {
        if (otherPeer != peer) {
            Client* otherClient = getClientForPeer(otherPeer);
            if (otherClient != nullptr) {
                sendMessage(otherClient, otherPeer, packet, reliabilityFlags, channel);
            }
        }
    }
}

void YNetServer::handlePacketToClient(Client* client, ENetPeer* peer, uint16_t targetClientId, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for packet to client" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Non-host peer attempting to send packet to specific client" << std::endl;
        }
        return;
    }

    // Find target client
    ENetPeer* targetPeer = nullptr;
    for (const auto& [otherPeer, clientId] : room.clients) {
        if (clientId == targetClientId) {
            targetPeer = otherPeer;
            break;
        }
    }

    if (targetPeer == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: Target client not found in room" << std::endl;
        }
        return;
    }

    // Forward packet to target client
    PacketMessage packet;
    packet.type = static_cast<uint8_t>(MessageType::PACKET_TO_CLIENT);
    packet.targetClientId = client->id;
    packet.packetData = packetData;
    packet.reliabilityFlags = reliabilityFlags; 
    packet.channel = channel;
    Client* targetClient = getClientForPeer(targetPeer);
    if (targetClient != nullptr) {
        sendMessage(targetClient, targetPeer, packet, reliabilityFlags, channel);
    }
}

void YNetServer::handleKickClient(Client* client, ENetPeer* peer, uint16_t targetClientId) {
    auto roomIt = rooms.find(client->currentRoom);
    if (roomIt == rooms.end()) {
        if (debugging) {
            std::cout << "DEBUG: Room not found for kick client" << std::endl;
        }
        return;
    }

    Room& room = roomIt->second;
    if (room.host != peer) {
        if (debugging) {
            std::cout << "DEBUG: Non-host peer attempting to kick client" << std::endl;
        }
        return;
    }

    // Find target client
    ENetPeer* targetPeer = nullptr;
    for (const auto& [otherPeer, clientId] : room.clients) {
        if (clientId == targetClientId) {
            targetPeer = otherPeer;
            break;
        }
    }

    if (targetPeer == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: Target client " << targetClientId << " not found in room" << std::endl;
        }
        return;
    }

    Client* targetClient = getClientForPeer(targetPeer);
    if (targetClient == nullptr) {
        if (debugging) {
            std::cout << "DEBUG: Target client not found" << std::endl;
        }
        return;
    }

    Message notification;
    notification.setMessageType(MessageType::ERROR);
    notification.setStringData("You have been kicked from the room");
    sendMessage(targetClient, targetPeer, notification);
    handleRoomLeave(targetClient, targetPeer);
    enet_peer_disconnect_later(targetPeer, 0);
    if (debugging) {
        std::cout << "DEBUG: Client " << targetClientId << " kicked from room" << std::endl;
    }
}

void YNetServer::handleJoinOrCreateRoom(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password) {
    if (debugging) {
        std::cout << "DEBUG: Handling join or create room request for code: " << roomCode << std::endl;
    }

    std::string finalRoomCode = roomCode;
    if (finalRoomCode.empty()) {
        finalRoomCode = generateRoomCode();
        if (debugging) {
            std::cout << "DEBUG: Generated random room code: " << finalRoomCode << std::endl;
        }
    }

    auto roomIt = rooms.find(finalRoomCode);
    if (roomIt != rooms.end()) {
        // Room exists, try to join it
        handleRoomJoin(client, peer, finalRoomCode, password);
    } else {
        // Room doesn't exist, create and join it
        handleRoomCreation(client, peer, finalRoomCode, password);
    }
}

void YNetServer::handleJoinOrCreateRoomRandom(Client* client, ENetPeer* peer) {
    uint32_t protocol = client->protocolVersion;
    if (debugging) {
        std::cout << "DEBUG: Handling join or create room random request for protocol: " << protocol << std::endl;
    }
    // Look for a non-private non-password-protected lobby with open spots
    for (const auto& [code, room] : rooms) {
        if (!room.isPrivate && 
            (room.password.empty()) && 
            room.protocol == client->protocolVersion && 
            room.clients.size() < static_cast<size_t>(room.maxPlayers)) {
            
            if (debugging) {
                std::cout << "DEBUG: Found matching room: " << code << std::endl;
            }
            
            // Join the found room
            handleRoomJoin(client, peer, code, "");
            return;
        }
    }

    // No suitable room found, create a new one
    std::string newRoomCode = generateRoomCode();
    if (debugging) {
        std::cout << "DEBUG: No suitable room found, creating new room: " << newRoomCode << std::endl;
    }
    
    handleRoomCreation(client, peer, newRoomCode, "");
}
