#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "world/chunk.h"

namespace yc {

class Persistence {

public:
    explicit Persistence(const std::string& worldFolder = "save/");
    ~Persistence();

    void setWorldFolder(const std::string& folder);
    const std::string& getWorldFolder() const { return worldFolder; }
    void reset(const std::string& folder);

    void saveChunk(std::shared_ptr<yc::world::Chunk> chunk);
    void syncRegionFiles();
    std::shared_ptr<yc::world::Chunk> getChunk(const glm::ivec2& chunkCoord, yc::world::World* world);

private:
    struct ChunkCoordHash {
        size_t operator()(const glm::ivec2& coord) const noexcept {
            size_t h = std::hash<int32_t>{}(coord.x);
            h ^= std::hash<int32_t>{}(coord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::shared_ptr<yc::world::Chunk> createChunkFromBytes(const glm::ivec2& chunkCoord, yc::world::World* world, const std::string& bytes) const;
    void enqueueSave(const glm::ivec2& coord, const std::string& raw);
    void workerLoop();

    std::string worldFolder;

    mutable std::mutex stateMutex;
    std::condition_variable queueCv;
    std::deque<glm::ivec2> writeQueue;
    std::unordered_set<glm::ivec2, ChunkCoordHash> queuedCoords;
    std::unordered_map<glm::ivec2, std::string, ChunkCoordHash> pendingWrites;
    std::unordered_map<glm::ivec2, std::string, ChunkCoordHash> chunkCache;

    std::vector<std::thread> workers;
    std::atomic<bool> stopWorkers{ false };

    static constexpr size_t MaxCachedChunks = 512;
};

}