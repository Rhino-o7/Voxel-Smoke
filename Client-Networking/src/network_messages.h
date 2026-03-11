#pragma once

#include <string>
#include <vector>
#include <optional>

#include "network_client.h"

namespace yc {

class NetworkMessages {
public:
    explicit NetworkMessages(NetworkClient& client) : client(client) {}

    std::vector<std::string> listSaves() const;
    bool createSave(const std::string& name) const;
    bool openSave(const std::string& name) const;
    bool deleteSave(const std::string& name) const;
    bool hasSaveData(const std::string& name) const;

    bool writeSaveData(const std::string& name, const std::string& payload) const;
    std::optional<std::string> readSaveData(const std::string& name) const;

    bool writeChunk(const std::string& saveName, int x, int z, const std::string& payload, int timeoutMs = 1200) const;
    std::optional<std::string> readChunk(const std::string& saveName, int x, int z, int timeoutMs = 1500) const;

private:
    static std::vector<std::string> split(const std::string& text, char delimiter);
    bool request(const std::string& command, std::vector<std::string>& responseParts, int timeoutMs = 5000) const;

    NetworkClient& client;
};

}
