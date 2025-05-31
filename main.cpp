#define ENET_IMPLEMENTATION
#include "thirdparty/enet.h"
#include "ynetserver.h"
#include "ynetclient.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>
#include <vector>

uint32_t stringToHash(const std::string& str) {
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

int main(int argc, char** argv) {
    // Check if we're running in test mode or client mode
    bool test_mode = false;
    bool client_mode = false;
    std::string server_address = "localhost:7777";  // Default address
    uint32_t protocol = 0;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--debug" || std::string(argv[i]) == "--d") {
            test_mode = true;
        } else if (std::string(argv[i]) == "--protocol" || std::string(argv[i]) == "--p") {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                protocol = stringToHash(std::string(argv[i + 1]));
                printf("Protocol: %u\n", protocol);
                i++; // Skip the next argument since we've used it
            }
        } else if (std::string(argv[i]) == "--client" || std::string(argv[i]) == "--c") {
            client_mode = true;
            // Check if there's an address argument after --client
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                server_address = argv[i + 1];
                i++; // Skip the next argument since we've used it
            }
        } else if (std::string(argv[i]) == "--ip") {
            // Check if there's an address argument after --client
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                server_address = argv[i + 1];
                i++; // Skip the next argument since we've used it
            }
        }
    }

    if (client_mode) {
        // ### CLIENT ###

        // Parse the server address
        std::string hostname = "localhost";
        uint16_t port = 7777;
        
        size_t colon_pos = server_address.find(':');
        if (colon_pos != std::string::npos) {
            hostname = server_address.substr(0, colon_pos);
            try {
                port = static_cast<uint16_t>(std::stoi(server_address.substr(colon_pos + 1)));
            } catch (const std::exception& e) {
                std::cerr << "Invalid port number in address: " << server_address << std::endl;
                enet_deinitialize();
                return 1;
            }
        }

        // Create and run client
        YNetClient client;
        client.debugging = test_mode;
        if (!client.connect(hostname, port, protocol)) {
            enet_deinitialize();
            return 1;
        }

        client.run();

        // Cleanup is handled by YNetClient destructor
        enet_deinitialize();
        return 0;
    } else {
        // ### SERVER ###

        // Normal server mode
        try {
            YNetServer server(7777, 8077);
            server.debugging = test_mode;
            server.run();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
            return 1;
        }
    }

    return 0;
}