#include "network_messages.h"

#include <websocketpp/base64/base64.hpp>

namespace yc {

std::vector<std::string> NetworkMessages::split(const std::string& text, char delimiter) {
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

bool NetworkMessages::request(const std::string& command, std::vector<std::string>& responseParts, int timeoutMs) const {
    std::string response;
    if (!client.request(command, response, timeoutMs)) {
        return false;
    }

    responseParts = split(response, '|');
    return !responseParts.empty() && responseParts[0] == "OK";
}

std::vector<std::string> NetworkMessages::listSaves() const {
    std::vector<std::string> parts;
    if (!request("LIST_SAVES", parts)) {
        return {};
    }

    std::vector<std::string> saves;
    if (parts.size() >= 2 && !parts[1].empty()) {
        const auto encoded = split(parts[1], ',');
        for (const auto& value : encoded) {
            if (!value.empty()) {
                saves.push_back(websocketpp::base64_decode(value));
            }
        }
    }

    return saves;
}

bool NetworkMessages::createSave(const std::string& name) const {
    std::vector<std::string> parts;
    return request("CREATE_SAVE|" + websocketpp::base64_encode(name), parts);
}

bool NetworkMessages::openSave(const std::string& name) const {
    std::vector<std::string> parts;
    return request("OPEN_SAVE|" + websocketpp::base64_encode(name), parts);
}

bool NetworkMessages::deleteSave(const std::string& name) const {
    std::vector<std::string> parts;
    return request("DELETE_SAVE|" + websocketpp::base64_encode(name), parts);
}

bool NetworkMessages::hasSaveData(const std::string& name) const {
    std::vector<std::string> parts;
    if (!request("HAS_SAVE_DATA|" + websocketpp::base64_encode(name), parts)) {
        return false;
    }

    return parts.size() >= 2 && parts[1] == "1";
}

bool NetworkMessages::writeSaveData(const std::string& name, const std::string& payload) const {
    std::vector<std::string> parts;
    return request(
        "WRITE_SAVE_DATA|" + websocketpp::base64_encode(name) + "|" + websocketpp::base64_encode(payload),
        parts
    );
}

std::optional<std::string> NetworkMessages::readSaveData(const std::string& name) const {
    std::vector<std::string> parts;
    if (!request("READ_SAVE_DATA|" + websocketpp::base64_encode(name), parts)) {
        return std::nullopt;
    }

    if (parts.size() < 2 || parts[1].empty()) {
        return std::nullopt;
    }

    return websocketpp::base64_decode(parts[1]);
}

bool NetworkMessages::writeChunk(const std::string& saveName, int x, int z, const std::string& payload, int timeoutMs) const {
    std::vector<std::string> parts;
    return request(
        "WRITE_CHUNK|" + websocketpp::base64_encode(saveName) + "|" + std::to_string(x) + "|" + std::to_string(z) + "|" + websocketpp::base64_encode(payload),
        parts,
        timeoutMs
    );
}

std::optional<std::string> NetworkMessages::readChunk(const std::string& saveName, int x, int z, int timeoutMs) const {
    std::vector<std::string> parts;
    if (!request(
        "READ_CHUNK|" + websocketpp::base64_encode(saveName) + "|" + std::to_string(x) + "|" + std::to_string(z),
        parts,
        timeoutMs
    )) {
        return std::nullopt;
    }

    if (parts.size() < 2 || parts[1].empty()) {
        return std::nullopt;
    }

    return websocketpp::base64_decode(parts[1]);
}

}
