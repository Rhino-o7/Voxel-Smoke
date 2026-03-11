#include "network_server.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <websocketpp/base64/base64.hpp>

std::vector<std::string> NetworkServer::split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t pos = text.find(delimiter, start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

NetworkServer::NetworkServer(uint16_t port) : port(port) {
    server.clear_access_channels(websocketpp::log::alevel::all);
    server.clear_error_channels(websocketpp::log::elevel::all);
}

bool NetworkServer::start() {
    server.init_asio();
    server.set_reuse_addr(true);

    server.set_message_handler([this](websocketpp::connection_hdl hdl, Server::message_ptr msg) {
        const std::string response = handleRequest(msg->get_payload());
        websocketpp::lib::error_code ec;
        server.send(hdl, response, websocketpp::frame::opcode::text, ec);
    });

    websocketpp::lib::error_code ec;
    server.listen(port, ec);
    if (ec) {
        return false;
    }

    server.start_accept();
    return true;
}

void NetworkServer::run() {
    std::cout << "Server listening on ws://0.0.0.0:" << port << std::endl;
    server.run();
}

std::filesystem::path NetworkServer::getSaveRoot() const {
    return std::filesystem::path("server_data") / "saves";
}

std::filesystem::path NetworkServer::getSavePath(const std::string& saveName) const {
    return getSaveRoot() / saveName;
}

std::filesystem::path NetworkServer::getChunkFolder(const std::string& saveName) const {
    return getSavePath(saveName) / "chunks";
}

std::filesystem::path NetworkServer::getChunkFile(const std::string& saveName, int x, int z) const {
    std::stringstream ss;
    ss << "chunk_" << x << "_" << z << ".bin";
    return getChunkFolder(saveName) / ss.str();
}

std::string NetworkServer::handleRequest(const std::string& request) {
    namespace fs = std::filesystem;

    const auto parts = split(request, '|');
    if (parts.empty()) {
        return "ERR|EMPTY";
    }

    const std::string& cmd = parts[0];
    std::error_code ec;

    if (cmd == "LIST_SAVES") {
        fs::create_directories(getSaveRoot(), ec);
        std::vector<std::string> names;
        for (const auto& entry : fs::directory_iterator(getSaveRoot(), ec)) {
            if (entry.is_directory()) {
                names.push_back(websocketpp::base64_encode(entry.path().filename().string()));
            }
        }

        std::string joined;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) {
                joined.push_back(',');
            }
            joined += names[i];
        }
        return "OK|" + joined;
    }

    if (cmd == "CREATE_SAVE") {
        if (parts.size() < 2) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        const fs::path savePath = getSavePath(saveName);
        fs::remove_all(savePath, ec);
        fs::create_directories(getChunkFolder(saveName), ec);
        return ec ? "ERR|FS" : "OK";
    }

    if (cmd == "OPEN_SAVE") {
        if (parts.size() < 2) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        fs::create_directories(getChunkFolder(saveName), ec);
        return ec ? "ERR|FS" : "OK";
    }

    if (cmd == "DELETE_SAVE") {
        if (parts.size() < 2) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        fs::remove_all(getSavePath(saveName), ec);
        return ec ? "ERR|FS" : "OK";
    }

    if (cmd == "HAS_SAVE_DATA") {
        if (parts.size() < 2) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        return fs::exists(getSavePath(saveName) / "save.dat", ec) ? "OK|1" : "OK|0";
    }

    if (cmd == "WRITE_SAVE_DATA") {
        if (parts.size() < 3) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        const std::string payload = websocketpp::base64_decode(parts[2]);
        fs::create_directories(getSavePath(saveName), ec);
        std::ofstream out(getSavePath(saveName) / "save.dat", std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return "ERR|IO";
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        return out.good() ? "OK" : "ERR|IO";
    }

    if (cmd == "READ_SAVE_DATA") {
        if (parts.size() < 2) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        const fs::path path = getSavePath(saveName) / "save.dat";
        if (!fs::exists(path, ec)) {
            return "ERR|NOT_FOUND";
        }

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return "ERR|IO";
        std::string payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return "OK|" + websocketpp::base64_encode(payload);
    }

    if (cmd == "WRITE_CHUNK") {
        if (parts.size() < 5) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        const int x = std::stoi(parts[2]);
        const int z = std::stoi(parts[3]);
        const std::string payload = websocketpp::base64_decode(parts[4]);
        fs::create_directories(getChunkFolder(saveName), ec);
        std::ofstream out(getChunkFile(saveName, x, z), std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return "ERR|IO";
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        return out.good() ? "OK" : "ERR|IO";
    }

    if (cmd == "READ_CHUNK") {
        if (parts.size() < 4) return "ERR|ARGS";
        const std::string saveName = websocketpp::base64_decode(parts[1]);
        const int x = std::stoi(parts[2]);
        const int z = std::stoi(parts[3]);
        const fs::path path = getChunkFile(saveName, x, z);
        if (!fs::exists(path, ec)) {
            return "ERR|NOT_FOUND";
        }

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return "ERR|IO";
        std::string payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return "OK|" + websocketpp::base64_encode(payload);
    }

    return "ERR|UNKNOWN";
}
