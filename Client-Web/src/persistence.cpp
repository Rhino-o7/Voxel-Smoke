#include "persistence.h"

#include <cstring>

#include "network_client.h"
#include "network_messages.h"

namespace yc {

Persistence::Persistence(const std::string& folder) : worldFolder(folder) {
}

Persistence::~Persistence() {
    syncRegionFiles();
}

void Persistence::setWorldFolder(const std::string& folder) {
    worldFolder = folder;
}

void Persistence::reset(const std::string& folder) {
    worldFolder = folder;
}

std::shared_ptr<yc::world::Chunk> Persistence::getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) {
    if (worldFolder.empty()) {
        return nullptr;
    }

    NetworkMessages messages(NetworkClient::instance());
    const auto bytesOpt = messages.readChunk(worldFolder, chunkCoord.x, chunkCoord.y, 1200);
    if (!bytesOpt.has_value()) {
        return nullptr;
    }

    return createChunkFromBytes(chunkCoord, world, bytesOpt.value());
}

std::shared_ptr<yc::world::Chunk> Persistence::createChunkFromBytes(const glm::ivec2& chunkCoord, yc::world::World* world, const std::string& bytes) const {
    if (bytes.size() != yc::world::Chunk::Volume * sizeof(yc::world::BlockData)) {
        return nullptr;
    }

    auto chunk = std::make_shared<yc::world::Chunk>();
    chunk->setCoordinate(world, chunkCoord);
    std::memcpy(chunk->getChunkData(), bytes.data(), bytes.size());
    return chunk;
}

void Persistence::saveChunk(std::shared_ptr<yc::world::Chunk> chunk) {
    if (!chunk || worldFolder.empty()) {
        return;
    }

    const glm::ivec2 coord = chunk->getCoord();
    const std::string raw(reinterpret_cast<const char*>(chunk->getChunkData()), yc::world::Chunk::Volume * sizeof(yc::world::BlockData));

    NetworkMessages messages(NetworkClient::instance());
    messages.writeChunk(worldFolder, coord.x, coord.y, raw, 900);
}

void Persistence::syncRegionFiles() {
}

}
