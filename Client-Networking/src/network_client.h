#pragma once

#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace yc {

class NetworkClient {
public:
    static NetworkClient& instance();

    bool connect(const std::string& uri, std::string* error = nullptr);
    void disconnect();

    bool isConnected() const { return connected.load(); }
    bool request(const std::string& payload, std::string& response, int timeoutMs = 5000);

private:
    using Client = websocketpp::client<websocketpp::config::asio_client>;

    NetworkClient();
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    void configureClient();

    std::unique_ptr<Client> client;
    websocketpp::connection_hdl connectionHdl;

    std::thread ioThread;
    std::atomic<bool> connected{ false };

    mutable std::mutex connectionMutex;

    std::mutex responseMutex;
    std::condition_variable responseCv;
    bool responseReady = false;
    std::string lastResponse;
};

}
