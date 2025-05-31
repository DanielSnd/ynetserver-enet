#pragma once
#include "thirdparty/enet.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <mutex>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <set>
#include <map>
#include <memory>

#include "mongoose.h"
#include "room.h"
#include "client.h"
#include "message.h"
#include <arpa/inet.h>
#include "thirdparty/json.hpp"

static uint32_t hash_fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

struct RoomFilters {
    int minPlayers = 0;
    int maxPlayers = INT_MAX;
    std::string nameContains = "";
    std::string extraInfoContains = "";
    int pageSize = 20;  // Default page size
    int pageNumber = 1; // Default page number
    static RoomFilters fromJson(const std::string& jsonStr);
};

class YNetServer {
private:
    ENetHost* server;
    std::map<ENetPeer*, Client*> clients;
    std::map<std::string, Room> rooms;
    std::map<uint16_t, std::string> roomsByID;
    std::mutex dataMutex;  // Mutex to protect shared data structures
    
    // Room ID management
    std::set<uint16_t> usedRoomIds;
    uint16_t nextRoomId = 1000;
    std::set<uint16_t> usedClientIds;
    uint16_t nextClientId = 1000;  // Counter for generating unique client IDs
    
    // Web server components
    struct mg_mgr mgr;
    struct mg_connection* web_server;
    
    uint16_t roomIdForPeer(ENetPeer* peer);
    Client* getClientForPeer(ENetPeer* peer);
    Client* createClientForPeer(ENetPeer* peer, uint32_t protocol);
    // Message handling
    void handleReceivedData(Client* client, ENetPeer* peer, const uint8_t* data, size_t dataLength);
    
    // Room management
    void handleRoomCreation(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password);
    void handleRoomJoin(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password);
    void handleRoomLeave(Client* client, ENetPeer* peer);
    void handleGetRoomList(Client* client, ENetPeer* peer, const std::string& filters);
    
    // Message handling
    void handleRPC(Client* client, ENetPeer* peer, const uint16_t& rpcType, const std::vector<uint8_t>& data);
    void broadcastMessage(Client* client, ENetPeer* broadcaster_peer, const std::string& message);
    void sendMessage(Client* client, ENetPeer* peer, BaseMessage& message, uint8_t reliabilityFlags = ENET_PACKET_FLAG_RELIABLE, uint8_t channel = 0);
    void sendRPCMessage(Client* client, ENetPeer* peer, RPCMessage& message);
    
    // Web server handlers
    static void handleWebRequest(struct mg_connection* c, int ev, void* ev_data);
    std::string generateStatusPage();
    
    // Helper functions
    std::string generateRoomCode();
    bool isValidRoomCode(const std::string& code);
    uint16_t generateRoomId();  // Generate a unique room ID
    void releaseRoomId(uint16_t id);  // Release a room ID back to the pool
    uint16_t generateClientId();  // Generate a unique client ID
    void releaseClientId(uint16_t id);  // Release a client ID back to the pool
    
    uint32_t stringToHash(const std::string& str);

    // New handler methods for additional message types
    void handleRPCToServer(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data);
    void handleRPCToClients(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data);
    void handleRPCToClient(Client* client, ENetPeer* peer, uint16_t rpcType, const std::vector<uint8_t>& data);
    void handleSetRoomPassword(Client* client, ENetPeer* peer, const std::string& newPassword);
    void handleSetMaxPlayers(Client* client, ENetPeer* peer, const std::string& newMaxPlayers);
    void handleSetRoomPrivate(Client* client, ENetPeer* peer, const std::string& newPrivate);
    void handleSetHostMigration(Client* client, ENetPeer* peer, const std::string& newCanHostMigrate);
    void handleSetRoomName(Client* client, ENetPeer* peer, const std::string& newRoomName);
    void handleSetExtraInfo(Client* client, ENetPeer* peer, const std::string& newExtraInfo);
    void handleGetRoomInfo(Client* client, ENetPeer* peer);
    void handlePacketToServer(Client* client, ENetPeer* peer, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel);
    void handlePacketToClients(Client* client, ENetPeer* peer, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel);
    void handlePacketToClient(Client* client, ENetPeer* peer, uint16_t targetClientId, const std::vector<uint8_t>& packetData, uint8_t reliabilityFlags, uint8_t channel);
    void handleKickClient(Client* client, ENetPeer* peer, uint16_t targetClientId);
    void handleJoinOrCreateRoom(Client* client, ENetPeer* peer, const std::string& roomCode, const std::string& password);
    void handleJoinOrCreateRoomRandom(Client* client, ENetPeer* peer);
    
public:
    bool debugging = false;
    YNetServer(uint16_t port = 8211, uint16_t web_port = 8212);
    ~YNetServer();
    void run();
}; 