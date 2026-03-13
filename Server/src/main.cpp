#include <iostream>
#include <string>
#include "network_server.h"

int main() {
    uint16_t port = 0;
    while (true) {
        std::cout << "Enter port to run server on (1-65535): ";
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "Failed to read input" << std::endl;
            return 1;
        }

        try {
            const int parsedPort = std::stoi(input);
            if (parsedPort >= 1 && parsedPort <= 65535) {
                port = static_cast<uint16_t>(parsedPort);
                break;
            }
        }
        catch (...) {
        }

        std::cout << "Invalid port. Please enter a number between 1 and 65535." << std::endl;
    }

    NetworkServer server(port);
    if (!server.start()) {
        std::cout << "Failed to start server" << std::endl;
        return 1;
    }

    server.run();
    return 0;
}
