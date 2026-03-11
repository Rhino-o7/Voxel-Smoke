#include <iostream>
#include "network_server.h"

int main() {
    NetworkServer server(9002);
    if (!server.start()) {
        std::cout << "Failed to start server" << std::endl;
        return 1;
    }

    server.run();
    return 0;
}
