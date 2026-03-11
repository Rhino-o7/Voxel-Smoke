#include "persistence.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "network_client.h"
#include "network_messages.h"

namespace yc {

Persistence::Persistence(const std::string& folder) : worldFolder(folder) {
    const unsigned int workerCount = std::max(1u, std::thread::hardware_concurrency() / 2);
    workers.reserve(workerCount);
    for (unsigned int i = 0; i < workerCount; ++i) {
        workers.emplace_back([this]() { workerLoop(); });
    }
}

Persistence::~Persistence() {
    syncRegionFiles();

    stopWorkers.store(true);
    queueCv.notify_all();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void Persistence::setWorldFolder(const std::string& folder) {
    syncRegionFiles();
    std::lock_guard<std::mutex> lock(stateMutex);
    worldFolder = folder;
}

void Persistence::reset(const std::string& folder) {
    syncRegionFiles();

    std::lock_guard<std::mutex> lock(stateMutex);
    worldFolder = folder;
    writeQueue.clear();
    queuedCoords.clear();
    pendingWrites.clear();
    chunkCache.clear();
}

std::shared_ptr<yc::world::Chunk> Persistence::getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) {
    std::string saveName;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (worldFolder.empty()) {
            return nullptr;
        }

        saveName = worldFolder;

        auto pending = pendingWrites.find(chunkCoord);
        if (pending != pendingWrites.end()) {
            return createChunkFromBytes(chunkCoord, world, pending->second);
        }

        auto cached = chunkCache.find(chunkCoord);
        if (cached != chunkCache.end()) {
            return createChunkFromBytes(chunkCoord, world, cached->second);
        }
    }

    NetworkMessages messages(NetworkClient::instance());
    const auto bytesOpt = messages.readChunk(saveName, chunkCoord.x, chunkCoord.y, 1200);
    if (!bytesOpt.has_value()) {
        return nullptr;
    }

    std::string bytes = bytesOpt.value();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (chunkCache.size() >= MaxCachedChunks) {
            chunkCache.clear();
        }
        chunkCache[chunkCoord] = bytes;
    }

    return createChunkFromBytes(chunkCoord, world, bytes);
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

void Persistence::enqueueSave(const glm::ivec2& coord, const std::string& raw) {
    std::lock_guard<std::mutex> lock(stateMutex);

    pendingWrites[coord] = raw;
    if (chunkCache.size() >= MaxCachedChunks) {
        chunkCache.clear();
    }
    chunkCache[coord] = raw;

    if (queuedCoords.insert(coord).second) {
        writeQueue.push_back(coord);
        queueCv.notify_one();
    }
}

void Persistence::saveChunk(std::shared_ptr<yc::world::Chunk> chunk) {
    if (!chunk) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (worldFolder.empty()) {
            return;
        }
    }

    const glm::ivec2 coord = chunk->getCoord();
    const std::string raw(reinterpret_cast<const char*>(chunk->getChunkData()), yc::world::Chunk::Volume * sizeof(yc::world::BlockData));
    enqueueSave(coord, raw);
}

void Persistence::workerLoop() {
    while (true) {
        glm::ivec2 coord{};
        std::string saveName;
        std::string payload;

        {
            std::unique_lock<std::mutex> lock(stateMutex);
            queueCv.wait(lock, [this]() {
                return stopWorkers.load() || !writeQueue.empty();
            });

            if (stopWorkers.load() && writeQueue.empty()) {
                return;
            }

            coord = writeQueue.front();
            writeQueue.pop_front();
            queuedCoords.erase(coord);

            auto it = pendingWrites.find(coord);
            if (it == pendingWrites.end()) {
                continue;
            }

            saveName = worldFolder;
            payload = it->second;
        }

        if (saveName.empty()) {
            continue;
        }

        NetworkMessages messages(NetworkClient::instance());
        const bool ok = messages.writeChunk(saveName, coord.x, coord.y, payload, 900);

        if (ok) {
            std::lock_guard<std::mutex> lock(stateMutex);
            auto it = pendingWrites.find(coord);
            if (it != pendingWrites.end() && it->second == payload) {
                pendingWrites.erase(it);
            }
        } else {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (pendingWrites.find(coord) != pendingWrites.end() && queuedCoords.insert(coord).second) {
                writeQueue.push_back(coord);
            }
        }

        queueCv.notify_all();
    }
}

void Persistence::syncRegionFiles() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (writeQueue.empty() && pendingWrites.empty()) {
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

}
