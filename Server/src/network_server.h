#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class NetworkServer {
public:
    explicit NetworkServer(uint16_t port);

    bool start();
    void run();

private:
    using Server = websocketpp::server<websocketpp::config::asio>;

    static std::vector<std::string> split(const std::string& text, char delimiter);
    std::string handleRequest(const std::string& request);

    std::filesystem::path getSaveRoot() const;
    std::filesystem::path getSavePath(const std::string& saveName) const;
    std::filesystem::path getChunkFolder(const std::string& saveName) const;
    std::filesystem::path getChunkFile(const std::string& saveName, int x, int z) const;

    Server server;
    uint16_t port = 0;
};
