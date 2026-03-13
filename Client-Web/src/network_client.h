#pragma once

#include <atomic>
#include <string>

namespace yc {

class NetworkClient {
public:
    static NetworkClient& instance();

    bool connect(const std::string& uri, std::string* error = nullptr);
    void disconnect();

    bool isConnected() const { return connected.load(); }
    bool request(const std::string& payload, std::string& response, int timeoutMs = 5000);

private:
    NetworkClient();
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    std::atomic<bool> connected{ false };
};

}
