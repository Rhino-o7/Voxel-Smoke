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
#include "settings.h"

namespace yc::world {

using WorldPos = glm::dvec3;
using BlockPos = glm::ivec3;

struct BlockPosHash {
    size_t operator() (const BlockPos& coord) const noexcept {
        size_t h = std::hash<int32_t>{}(coord.x);
        h ^= std::hash<int32_t>{}(coord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int32_t>{}(coord.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

using CropExposureMap = std::unordered_map<BlockPos, double, BlockPosHash>;

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

    World() : generator(0), persistence(nullptr), exposureScale(0.0f) {}
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


    void setWindState(const WindState& state) { windState = state; }
    const WindState& getWindState() const { return windState; }
    const std::vector<ChimneySource>& getChimneyEmitters() const { return chimneyEmitters; }

    void setSimTimeSec(double value) { simTimeSec = value; }
    double getSimTimeSec() const { return simTimeSec; }

    void setSmokeSettings(const yc::Settings::SmokeSettings& value) { smokeSettings = value; }
    const yc::Settings::SmokeSettings& getSmokeSettings() const { return smokeSettings; }

    void setCropExposureMap(const CropExposureMap* map);
    double getCropExposureAtBlock(const BlockPos& blockPos) const;

    void setSettings(const yc::Settings::WorldSettings& value) { settings = value; }
    const yc::Settings::WorldSettings& getSettings() const { return settings; }

    void setExposureScale(float value) { exposureScale = value; }
    float getExposureScale() const { return exposureScale; }

    void clearChunks();
    void setChimneyEmitters(const std::vector<ChimneySource>& emitters);
    void clearChimneyEmitters();

private:
    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>, HashChunkCoord> chunks;
    std::queue<std::shared_ptr<Chunk>> shouldBeUnloadedChunks;

    WorldGenerator generator;
    Persistence* persistence;

    WindState windState{};
    double simTimeSec = 0.0;

    std::vector<ChimneySource> chimneyEmitters;

    const CropExposureMap* cropExposureMap = nullptr;

    yc::Settings::WorldSettings settings{};
    yc::Settings::SmokeSettings smokeSettings{};
    float exposureScale;
};

}
