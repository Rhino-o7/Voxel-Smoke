#include "network_client.h"

#include <chrono>
#include <websocketpp/common/thread.hpp>

namespace yc {

NetworkClient& NetworkClient::instance() {
    static NetworkClient instance;
    return instance;
}

NetworkClient::NetworkClient() {
    configureClient();
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& uri, std::string* error) {
    disconnect();

    configureClient();

    client->start_perpetual();
    ioThread = std::thread([this]() {
        client->run();
    });

    websocketpp::lib::error_code ec;
    auto con = client->get_connection(uri, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        disconnect();
        return false;
    }

    client->connect(con);

    for (int i = 0; i < 200; ++i) {
        if (connected.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (error) {
        *error = "Connection timeout";
    }

    disconnect();
    return false;
}

void NetworkClient::disconnect() {
    if (!client) {
        connected.store(false);
        return;
    }

    if (connected.load()) {
        websocketpp::lib::error_code ec;
        client->close(connectionHdl, websocketpp::close::status::normal, "", ec);
    }

    connected.store(false);
    client->stop_perpetual();
    client->stop();

    if (ioThread.joinable()) {
        ioThread.join();
    }

    client.reset();
}

void NetworkClient::configureClient() {
    client = std::make_unique<Client>();

    client->clear_access_channels(websocketpp::log::alevel::all);
    client->clear_error_channels(websocketpp::log::elevel::all);
    client->init_asio();

    client->set_open_handler([this](websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(connectionMutex);
        connectionHdl = hdl;
        connected.store(true);
    });

    client->set_close_handler([this](websocketpp::connection_hdl) {
        connected.store(false);
    });

    client->set_fail_handler([this](websocketpp::connection_hdl) {
        connected.store(false);
    });

    client->set_message_handler([this](websocketpp::connection_hdl, Client::message_ptr msg) {
        std::lock_guard<std::mutex> lock(responseMutex);
        lastResponse = msg->get_payload();
        responseReady = true;
        responseCv.notify_one();
    });
}

bool NetworkClient::request(const std::string& payload, std::string& response, int timeoutMs) {
    if (!connected.load() || !client) {
        return false;
    }

    std::unique_lock<std::mutex> lock(responseMutex);
    responseReady = false;
    lastResponse.clear();

    websocketpp::lib::error_code ec;
    client->send(connectionHdl, payload, websocketpp::frame::opcode::text, ec);
    if (ec) {
        return false;
    }

    const bool gotResponse = responseCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
        return responseReady;
    });

    if (!gotResponse) {
        return false;
    }

    response = lastResponse;
    return true;
}

}
