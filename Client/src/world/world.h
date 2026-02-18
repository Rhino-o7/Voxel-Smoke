#pragma once

#include <unordered_map>
#include <queue>
#include <vector>
#include <glm/glm.hpp>
#include <cstdint>

#include "persistence.h"
#include "world/world_generator.h"
#include "world/chunk.h"
#include "player.h"
#include "world/wind_system.h"
#include "world/pollution_system.h"

namespace yc::world {

using WorldPos = glm::dvec3;
using BlockPos = glm::ivec3;

class World {
public:
    struct HashChunkCoord {
        size_t operator() (const glm::ivec2& coord) const noexcept;
    };

    struct RayCastResult {
        BlockData block;
        glm::ivec3 coord;
        glm::ivec3 face;
    };

    static glm::ivec2 GetChunkCoordOf(const glm::ivec3& coord);

    World() : generator(0), persistence(nullptr) {}
    World(Persistence* persistence);

    void init();
    void update(yc::Camera* camera);

    void renderOpaque(yc::Camera* camera);
    void renderTransparent(yc::Camera* camera);
    void renderFlora(yc::Camera* camera);

    BlockData getBlockDataIfLoadedAt(const glm::ivec3& coord);

    static BlockPos getWorldtoBlockCoord(const WorldPos& worldCoord);
    static WorldPos getBlockToWorldCoord(const BlockPos& blockCoord);

    bool setBlockDataIfLoadedAt(const glm::ivec3& coord, const BlockData& blockData);
    bool destroyBlockIfLoaded(const glm::ivec3& coord);

    std::shared_ptr<Chunk> getChunkIfLoadedAt(const glm::ivec2& coord);
    bool isChunkLoaded(const glm::ivec2& chunkCoord);

    void generateOrLoadChunkAt(const glm::ivec2& chunkCoord);
    void unloadChunk(const glm::ivec2& chunkCoord);

    void reloadChunks();
    void saveChunks();

    void spawnTreeAt(const glm::ivec3& coord);
    void spawnChimneyAt(const glm::ivec3& coord, int height, int radius, double exitVelocity);

    RayCastResult raycastCheck(const glm::vec3& position, const glm::vec3& direction, bool discardFlora, bool discardWater);

    int32_t getSeed() const;

    void addChimneyEmitter(const BlockPos& baseBlockCoord, double height, double exitVelocity, double radius);

    void clearAllSmokeBlocksInLoadedChunks();

    void setWindState(const WindState& state) { windState = state; }
    const WindState& getWindState() const { return windState; }
    const std::vector<ChimneySource>& getChimneyEmitters() const { return chimneyEmitters; }

private:
    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>, HashChunkCoord> chunks;
    std::queue<std::shared_ptr<Chunk>> shouldBeUnloadedChunks;

    WorldGenerator generator;
    Persistence* persistence;

    WindState windState{};

    std::vector<ChimneySource> chimneyEmitters;
};

}
