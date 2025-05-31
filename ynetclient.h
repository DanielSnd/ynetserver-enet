#pragma once

#include "thirdparty/enet.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "message.h"
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

class YNetClient {
private:
    ENetHost* client;
    ENetPeer* peer;
    std::string currentRoom;
    uint16_t currentRoomId;
    std::unordered_map<std::string, uint16_t> roomIdMap;  // Maps room codes to room IDs
    std::thread messageThread;
    std::atomic<bool> running;
    std::mutex messageMutex;
    std::condition_variable messageCV;
    int protocolVersion;  // Protocol version as integer

    bool setupClient();
    bool sendMessage(BaseMessage& msg);
    void messageLoop();

public:
    YNetClient(uint32_t protocolVersion = 0);  // Constructor with protocol version
    ~YNetClient();

    bool debugging = false;
    
    bool connect(const std::string& hostname = "localhost", uint16_t port = 7777, uint32_t protocolVersion = 0);
    void disconnect();
    void run();
    
    // Command handlers
    bool createRoom(const std::string& roomCode);
    bool joinRoom(const std::string& roomCode, const std::string& password = "");
    bool leaveRoom();
    bool sendRoomMessage(const std::string& message);
    
    // New room management methods
    bool createRoomWithPassword(const std::string& roomCode, const std::string& password);
    bool joinRoomWithPassword(const std::string& roomCode, const std::string& password);
    bool joinOrCreateRoom(const std::string& roomCode, const std::string& password = "");
    bool joinOrCreateRoomRandom();
    bool setRoomPassword(const std::string& password);
    bool setMaxPlayers(int maxPlayers);
    bool setRoomPrivate(bool isPrivate);
    bool setHostMigration(bool canHostMigrate);
    bool setRoomName(const std::string& name);
    bool setExtraInfo(const std::string& info);
    bool getRoomInfo();
    bool getRoomList();
    bool kickClient(const std::string& clientId);
    
    // RPC methods
    bool sendRPCToServer(uint16_t rpcType, const std::vector<uint8_t>& data);
    bool sendRPCToClients(uint16_t rpcType, const std::vector<uint8_t>& data);
    bool sendRPCToClient(const std::string& targetClientId, uint16_t rpcType, const std::vector<uint8_t>& data);
    
    // Packet methods
    bool sendPacketToServer(const std::vector<uint8_t>& packetData);
    bool sendPacketToClients(const std::vector<uint8_t>& packetData);
    bool sendPacketToClient(const std::string& targetClientId, const std::vector<uint8_t>& packetData);
    
    // Getters
    const std::string& getCurrentRoom() const { return currentRoom; }
    bool isConnected() const { return peer != nullptr; }
    int getProtocolVersion() const { return protocolVersion; }
}; 