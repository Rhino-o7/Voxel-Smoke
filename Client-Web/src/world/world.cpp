#include <iostream>
#include <atomic>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "glm/gtc/matrix_transform.hpp"
#include "world/world.h"
#include "util/math.h"
#include "resource.h"
#include "graphic/lighting.h"
#include <sstream>
#include <array>
#include <algorithm>

namespace yc::world {

static float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

struct ViewFrustum {
    std::array<glm::vec4, 6> planes{};
};

static void NormalizePlane(glm::vec4& plane) {
    const glm::vec3 normal{ plane.x, plane.y, plane.z };
    const float length = glm::length(normal);
    if (length <= 0.0f) {
        return;
    }

    plane /= length;
}

static ViewFrustum BuildFrustumFromMatrix(const glm::mat4& matrix) {
    ViewFrustum frustum{};

    frustum.planes[0] = {
        matrix[0][3] + matrix[0][0],
        matrix[1][3] + matrix[1][0],
        matrix[2][3] + matrix[2][0],
        matrix[3][3] + matrix[3][0]
    }; // Left
    frustum.planes[1] = {
        matrix[0][3] - matrix[0][0],
        matrix[1][3] - matrix[1][0],
        matrix[2][3] - matrix[2][0],
        matrix[3][3] - matrix[3][0]
    }; // Right
    frustum.planes[2] = {
        matrix[0][3] + matrix[0][1],
        matrix[1][3] + matrix[1][1],
        matrix[2][3] + matrix[2][1],
        matrix[3][3] + matrix[3][1]
    }; // Bottom
    frustum.planes[3] = {
        matrix[0][3] - matrix[0][1],
        matrix[1][3] - matrix[1][1],
        matrix[2][3] - matrix[2][1],
        matrix[3][3] - matrix[3][1]
    }; // Top
    frustum.planes[4] = {
        matrix[0][3] + matrix[0][2],
        matrix[1][3] + matrix[1][2],
        matrix[2][3] + matrix[2][2],
        matrix[3][3] + matrix[3][2]
    }; // Near
    frustum.planes[5] = {
        matrix[0][3] - matrix[0][2],
        matrix[1][3] - matrix[1][2],
        matrix[2][3] - matrix[2][2],
        matrix[3][3] - matrix[3][2]
    }; // Far

    for (auto& plane : frustum.planes) {
        NormalizePlane(plane);
    }

    return frustum;
}

static bool IsChunkVisibleInFrustum(const glm::ivec2& chunkCoord, const ViewFrustum& frustum) {
    const glm::vec3 minCorner{
        static_cast<float>(chunkCoord.x * Chunk::Length),
        0.0f,
        static_cast<float>(chunkCoord.y * Chunk::Width)
    };
    const glm::vec3 maxCorner{
        minCorner.x + static_cast<float>(Chunk::Length),
        static_cast<float>(Chunk::Height),
        minCorner.z + static_cast<float>(Chunk::Width)
    };

    for (const auto& plane : frustum.planes) {
        const glm::vec3 positiveVertex{
            (plane.x >= 0.0f) ? maxCorner.x : minCorner.x,
            (plane.y >= 0.0f) ? maxCorner.y : minCorner.y,
            (plane.z >= 0.0f) ? maxCorner.z : minCorner.z
        };

        if (glm::dot(glm::vec3(plane), positiveVertex) + plane.w < 0.0f) {
            return false;
        }
    }

    return true;
}

bool World::removeChimneyEmitterContainingBlock(const BlockPos& blockCoord) {
    for (size_t i = 0; i < chimneyEmitters.size(); ++i) {
        const auto& source = chimneyEmitters[i];
        const int height = std::max(1, static_cast<int>(std::round(source.height)));
        const int radius = std::max(1, static_cast<int>(std::round(source.radius)));
        const BlockPos base = source.baseBlockCoord;

        const bool insideY = blockCoord.y >= base.y && blockCoord.y < base.y + height;
        const int dx = blockCoord.x - base.x;
        const int dz = blockCoord.z - base.z;
        const bool insideRadius = (dx * dx + dz * dz) <= (radius * radius);

        if (insideY && insideRadius) {
            return removeChimneyEmitterAt(i);
        }
    }

    return false;
}

void World::addChimneyEmitter(const BlockPos& baseBlockCoord, double height, double exitVelocity, double radius) {
    ChimneySource src{};
    src.worldPos = getBlockToWorldCoord(baseBlockCoord);
    src.baseBlockCoord = baseBlockCoord;
    src.height = height;
    src.exitVelocity = exitVelocity;
    src.radius = radius;
    src.enabled = true;
    chimneyEmitters.push_back(src);
}

bool World::removeChimneyEmitterAt(size_t index) {
    if (index >= chimneyEmitters.size()) {
        return false;
    }

    const ChimneySource source = chimneyEmitters[index];
    const BlockPos base = source.baseBlockCoord;
    const int height = std::max(1, static_cast<int>(std::round(source.height)));
    const int radius = std::max(1, static_cast<int>(std::round(source.radius)));

    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>, HashChunkCoord> loadedTouchedChunks;
    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>, HashChunkCoord> diskTouchedChunks;

    for (int y = 0; y < height; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if ((x * x + z * z) <= (radius * radius)) {
                    const BlockPos worldBlock{ base.x + x, base.y + y, base.z + z };
                    const glm::ivec2 chunkCoord = GetChunkCoordOf(worldBlock);
                    const glm::ivec3 localCoord{
                        util::PositiveMod(worldBlock.x, Chunk::Length),
                        worldBlock.y,
                        util::PositiveMod(worldBlock.z, Chunk::Width)
                    };

                    auto loadedChunk = getChunkIfLoadedAt(chunkCoord);
                    if (loadedChunk) {
                        loadedChunk->setBlockData(localCoord, { yc::world::BlockType::AIR });
                        loadedTouchedChunks[chunkCoord] = loadedChunk;
                        continue;
                    }

                    auto diskChunkIt = diskTouchedChunks.find(chunkCoord);
                    std::shared_ptr<Chunk> diskChunk = (diskChunkIt != diskTouchedChunks.end())
                        ? diskChunkIt->second
                        : persistence->getChunk(chunkCoord, this);

                    if (!diskChunk) {
                        continue;
                    }

                    diskChunk->setBlockData(localCoord, { yc::world::BlockType::AIR });
                    diskTouchedChunks[chunkCoord] = diskChunk;
                }
            }
        }
    }

    for (auto& [coord, chunk] : loadedTouchedChunks) {
        if (!chunk) {
            continue;
        }

        chunk->prepareToBuildMesh();

        auto westChunk = getChunkIfLoadedAt({ coord.x - 1, coord.y });
        if (westChunk) westChunk->prepareToBuildMesh();
        auto eastChunk = getChunkIfLoadedAt({ coord.x + 1, coord.y });
        if (eastChunk) eastChunk->prepareToBuildMesh();
        auto northChunk = getChunkIfLoadedAt({ coord.x, coord.y - 1 });
        if (northChunk) northChunk->prepareToBuildMesh();
        auto southChunk = getChunkIfLoadedAt({ coord.x, coord.y + 1 });
        if (southChunk) southChunk->prepareToBuildMesh();
    }

    for (auto& [coord, chunk] : diskTouchedChunks) {
        if (chunk) {
            persistence->saveChunk(chunk);
        }
    }

    chimneyEmitters.erase(chimneyEmitters.begin() + index);
    return true;
}

bool World::setChimneyEmitterEnabledAt(size_t index, bool enabled) {
    if (index >= chimneyEmitters.size()) {
        return false;
    }

    chimneyEmitters[index].enabled = enabled;
    return true;
}

World::World(IPersistence* persistence) : generator(0), persistence(persistence) {}

void World::init()
{
    stopWorkers.store(false);
}

World::~World() {
    stopWorkers.store(true);
    jobQueueCv.notify_all();
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }
}

void World::enqueueJob(const std::function<void()>& job) {
    if (!job) {
        return;
    }

    try {
        job();
    } catch (...) {
        // swallow exceptions from jobs
    }
}

void World::update(yc::Camera* camera) {
    const glm::vec3 cameraPos = camera->getPosition();
    const BlockPos cameraBlockCoord = getWorldtoBlockCoord(WorldPos{
        static_cast<double>(cameraPos.x),
        static_cast<double>(cameraPos.y),
        static_cast<double>(cameraPos.z)
    });
    const glm::ivec2 cameraChunkCoord = GetChunkCoordOf(cameraBlockCoord);

    const int32_t viewDistance = settings.viewDistance;
    const int32_t maxUnloadChunkPerFrame = settings.maxUnloadChunkPerFrame;
    const int32_t maxChunksLoadPerFrame = settings.maxChunksLoadPerFrame;

    int32_t unloadedChunkCount = 0;
    for (const auto& [chunkCoord, chunk]: this->chunks) {
        // Don't unload chunks that are currently being built or have staging data ready
        if (Chunk::DistanceTo(chunkCoord, cameraChunkCoord) > viewDistance && maxUnloadChunkPerFrame > unloadedChunkCount) {
            if (chunk->isCurrentlyBuilding() || chunk->hasStagingData()) {
                // keep it until build/upload finishes
                continue;
            }
            shouldBeUnloadedChunks.push(chunk);
            unloadedChunkCount += 1;
        }
    }

    while (!shouldBeUnloadedChunks.empty()) {
        auto chunk = shouldBeUnloadedChunks.front();
        shouldBeUnloadedChunks.pop();
        unloadChunk(chunk->getCoord());
    }
    persistence->syncRegionFiles();

    std::atomic_int chunkCount = 0;

    int32_t x = 0, z = 0, dx = 0, dz = -1;
    int32_t size = viewDistance * 2 + 1;
    int32_t numChunksToCheck = size * size;

    while (numChunksToCheck-- && chunkCount < maxChunksLoadPerFrame) {
        glm::ivec2 chunkCoordToCheck = glm::ivec2(x, z) + cameraChunkCoord;

        if (Chunk::DistanceTo(chunkCoordToCheck, cameraChunkCoord) <= viewDistance) {
            auto iter = chunks.find(chunkCoordToCheck);
            if (iter == chunks.end() || iter->second == nullptr) {
                if (iter != chunks.end()) {
                    chunks.erase(iter);
                }
                generateOrLoadChunkAt(chunkCoordToCheck);
                ++chunkCount;
            }
        }

        if (x == z || (x < 0 && x == -z) || (x > 0 && x == 1 - z)) {
            int32_t t = dx;
            dx = -dz;
            dz = t;
        }

        x += dx;
        z += dz;
    }

    for (const auto& [chunkCoord, chunk]: this->chunks) {
        // If chunk has staging data ready, upload it before attempting new builds.
        if (chunk->hasStagingData()) {
            chunk->uploadMeshFromStaging();
            // after upload, it's safe to possibly schedule new build if prepareToBuildMesh was called
        }
        chunk->buildMeshIfNeeded();
    }
}

void World::generateOrLoadChunkAt(const glm::ivec2& chunkCoord) {
    auto chunk = persistence->getChunk(chunkCoord, this);

    if (chunk == nullptr) {
        chunk = this->generator.generateChunk(this, chunkCoord);
    }

    // Ensure it will render even if loaded-from-disk
    chunk->prepareToBuildMesh();

    const std::array<glm::ivec2, 4> neighborChunks = {{
        { +1, 0 }, { -1, 0 }, { 0, +1 }, { 0, -1 },
    }};

    for (auto& neighbor: neighborChunks) {
        auto neighborChunkCoord = chunkCoord + neighbor;
        if (isChunkLoaded(neighborChunkCoord)) {
            this->chunks[neighborChunkCoord]->prepareToBuildMesh();
        }
    }

    this->chunks[chunkCoord] = chunk;
}

void World::unloadChunk(const glm::ivec2& chunkCoord) {
    auto iter = this->chunks.find(chunkCoord);
    if (iter == this->chunks.end() || iter->second == nullptr) {
        return;
    }

    persistence->saveChunk(iter->second);
    this->chunks.erase(iter);
}

bool World::isChunkLoaded(const glm::ivec2& chunkCoord) {
    auto iter = this->chunks.find(chunkCoord);
    return iter != this->chunks.end() && iter->second != nullptr;
}

void World::setCropExposureMap(const CropExposureMap* map) {
    cropExposureMap = map;
}

double World::getCropExposureAtBlock(const BlockPos& blockPos) const {
    if (!cropExposureMap) return 0.0;

    auto it = cropExposureMap->find(blockPos);
    return (it != cropExposureMap->end()) ? it->second : 0.0;
}

std::vector<BlockPos> World::getLoadedBlockPositionsOfType(BlockType type) const {
    std::vector<BlockPos> result;

    for (const auto& [chunkCoord, chunk] : this->chunks) {
        (void)chunkCoord;
        if (!chunk) {
            continue;
        }

        BlockData* data = chunk->getChunkData();
        if (!data) {
            continue;
        }

        for (int x = 0; x < Chunk::Length; ++x) {
            for (int y = 0; y < Chunk::Height; ++y) {
                for (int z = 0; z < Chunk::Width; ++z) {
                    const int idx = x * (Chunk::Height * Chunk::Width) + y * Chunk::Width + z;
                    if (data[idx].getType() != type) {
                        continue;
                    }

                    result.push_back(chunk->getWorldCoordOfBlock({ x, y, z }));
                }
            }
        }
    }

    return result;
}

World::CullingStats World::getCullingStats() const {
    CullingStats stats{};
    stats.loadedChunks = chunks.size();
    stats.visibleChunks = visibleChunkCount;
    stats.culledChunks = (stats.loadedChunks > stats.visibleChunks)
        ? (stats.loadedChunks - stats.visibleChunks)
        : 0;
    return stats;
}

void World::renderOpaque(Camera* camera) {
    yc::Resource::GameTexure.bind();
    
    yc::Resource::OpaqueShader.use();
    const glm::mat4 projectionView = camera->getProjectionViewMatrix();
    const ViewFrustum frustum = BuildFrustumFromMatrix(projectionView);
    const auto lightingState = yc::graphic::lighting::BuildLightingState(simTimeSec, lightingSettings);

    yc::Resource::OpaqueShader.setMat4("projection_view", projectionView);
    yc::Resource::OpaqueShader.setFloat("uExposureScale", exposureScale);
    yc::graphic::lighting::ApplyLightingUniforms(yc::Resource::OpaqueShader, lightingState, camera->getPosition());

    visibleChunkCount = 0;

    for (const auto& [coord, chunk]: this->chunks) {
        if (!IsChunkVisibleInFrustum(coord, frustum)) {
            continue;
        }

        ++visibleChunkCount;

        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(coord.x * Chunk::Length, 0, coord.y * Chunk::Width));
        yc::Resource::OpaqueShader.setMat4("model", model);
        chunk->renderOpaque();
    }
}

void World::renderTransparent(Camera* camera) {
    yc::Resource::GameTexure.bind();

    yc:: Resource::TransparentShader.use();
    const glm::mat4 projectionView = camera->getProjectionViewMatrix();
    const ViewFrustum frustum = BuildFrustumFromMatrix(projectionView);
    const auto lightingState = yc::graphic::lighting::BuildLightingState(simTimeSec, lightingSettings);

    yc:: Resource::TransparentShader.setMat4("projection_view", projectionView);
    yc:: Resource::TransparentShader.setFloat("uExposureScale", exposureScale);
    yc::Resource::TransparentShader.setVec3("uWaterTint", glm::vec3(lightingSettings.waterTintR, lightingSettings.waterTintG, lightingSettings.waterTintB));
    yc::Resource::TransparentShader.setFloat("uWaterDiffuseMul", lightingSettings.waterDiffuseMul);
    yc::Resource::TransparentShader.setFloat("uWaterSpecularMul", lightingSettings.waterSpecularMul);
    yc::Resource::TransparentShader.setFloat("uWaterMinAlpha", lightingSettings.waterMinAlpha);
    yc::graphic::lighting::ApplyLightingUniforms(yc::Resource::TransparentShader, lightingState, camera->getPosition());

    for (const auto& [coord, chunk]: this->chunks) {
        if (!IsChunkVisibleInFrustum(coord, frustum)) {
            continue;
        }

        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(coord.x * Chunk::Length, 0, coord.y * Chunk::Width));
        yc:: Resource::TransparentShader.setMat4("model", model);
        chunk->renderTransparent();
    }
}

void World::renderFlora(yc::Camera* camera) {
    yc::Resource::GameTexure.bind();

    yc::Resource::FloraShader.use();
    const glm::mat4 projectionView = camera->getProjectionViewMatrix();
    const ViewFrustum frustum = BuildFrustumFromMatrix(projectionView);
    const auto lightingState = yc::graphic::lighting::BuildLightingState(simTimeSec, lightingSettings);

    yc::Resource::FloraShader.setMat4("projection_view", projectionView);
    yc::graphic::lighting::ApplyLightingUniforms(yc::Resource::FloraShader, lightingState, camera->getPosition());

    for (const auto& [coord, chunk]: this->chunks) {
        if (!IsChunkVisibleInFrustum(coord, frustum)) {
            continue;
        }

        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(coord.x * Chunk::Length, 0, coord.y * Chunk::Width));
        yc::Resource::FloraShader.setMat4("model", model);
        chunk->renderFlora();
    }
}   

void World::saveChunks() {
    std::cout << "Saving world . . .\n";
    for (auto& [coord, chunk]: this->chunks) {
        persistence->saveChunk(chunk);
    }
    persistence->syncRegionFiles();
}

BlockData World::getBlockDataIfLoadedAt(const glm::ivec3& coord) {
    if (coord.y >= 0 && coord.y < 256) {
        glm::ivec2 chunkCoord = GetChunkCoordOf(coord);

        // shared lock for read access
        std::shared_lock<std::shared_mutex> lock(this->chunksMutex);
        auto iter = this->chunks.find(chunkCoord);
        if (iter != this->chunks.end()) {
            // copy shared_ptr while holding lock so it stays valid after we release
            auto chunk = iter->second;
            lock.unlock();

            return chunk->getBlockDataAt({
                yc::util::PositiveMod(coord.x, Chunk::Length),
                coord.y,
                yc::util::PositiveMod(coord.z, Chunk::Width)
                });
        }
        // lock released on scope exit if not already unlocked
    }

    return { BlockType::NONE, BlockFaceDirection::NONE };
}

BlockPos World::getWorldtoBlockCoord(const WorldPos& worldCoord) {
    // Map a continuous world position to the containing voxel (block index).
    // floor() is required for correct negative coordinate behavior:
    // world x = -0.1 should map to block -1, not 0.
    return BlockPos{
        static_cast<int32_t>(std::floor(worldCoord.x)),
        static_cast<int32_t>(std::floor(worldCoord.y)),
        static_cast<int32_t>(std::floor(worldCoord.z))
    };
}

WorldPos World::getBlockToWorldCoord(const BlockPos& blockCoord) {
    // Map a voxel coordinate to world-space position of the block origin (min corner).
    // If you want the block center instead, add 0.5 to each component.
    return WorldPos{
        static_cast<double>(blockCoord.x),
        static_cast<double>(blockCoord.y),
        static_cast<double>(blockCoord.z)
    };
}

// no thread-safe
void World::reloadChunks() {
    std::cout << this->chunks.size() << '\n';
    double begin = glfwGetTime();
    for (auto& [chunkCoord, chunk]: this->chunks) {
        chunk->buildMesh();
    }
    double end = glfwGetTime();

    std::cout << "Reloaded chunks in " << end - begin << " s\n";
}

// no thread-safe
std::shared_ptr<Chunk> World::getChunkIfLoadedAt(const glm::ivec2& coord) {
    auto iter = this->chunks.find(coord);

    if (iter != this->chunks.end()) {
        return iter->second;
    } else {
        return nullptr;
    }
}

glm::ivec2 World::GetChunkCoordOf(const glm::ivec3& coord) {
    glm::ivec2 chunkCoord;

    if (coord.x >= 0) {
        chunkCoord.x = coord.x / Chunk::Length;
    } else {
        chunkCoord.x = (coord.x+1) / Chunk::Length - 1;
    }

    if (coord.z >= 0) {
        chunkCoord.y = coord.z / Chunk::Width;
    } else {
        chunkCoord.y = (coord.z+1) / Chunk::Width - 1;
    }

    return chunkCoord;
}



bool World::destroyBlockIfLoaded(const glm::ivec3& coord) {
    return setBlockDataIfLoadedAt(coord, { yc::world::BlockType::AIR });
}

bool World::setBlockDataIfLoadedAt(const glm::ivec3& coord, const BlockData& blockData) {
    if (coord.y<0 || coord.y>=256) return false;

    glm::ivec2 chunkCoord = GetChunkCoordOf(coord);

    auto iter = this->chunks.find(chunkCoord);

    if (iter != this->chunks.end()) {
        auto chunk = iter->second;

        chunk->setBlockData({
            util::PositiveMod(coord.x, Chunk::Length),
            coord.y,
            util::PositiveMod(coord.z, Chunk::Width)
        }, blockData);

        chunk->prepareToBuildMesh();

        glm::ivec3 localCoord = { coord.x & 15, coord.y, coord.z & 15 };

        if (localCoord.x == 0) {
            auto westChunk = getChunkIfLoadedAt({ chunkCoord.x-1, chunkCoord.y });
            if (westChunk) westChunk->prepareToBuildMesh();
        } else if (localCoord.x == 15) {
            auto eastChunk = getChunkIfLoadedAt({ chunkCoord.x+1, chunkCoord.y });
            if (eastChunk) eastChunk->prepareToBuildMesh();
        }

        if (localCoord.z == 0) {
            auto northChunk = getChunkIfLoadedAt({ chunkCoord.x, chunkCoord.y-1 });
            if (northChunk) northChunk->prepareToBuildMesh();
        } else if (localCoord.z == 15) {
            auto southChunk = getChunkIfLoadedAt({ chunkCoord.x, chunkCoord.y+1 });
            if (southChunk) southChunk->prepareToBuildMesh();
        }

        return true;
    } else {    
        return false;
    }
}

void World::spawnTreeAt(const glm::ivec3& coord) {
    generator.generateTreeAt(this, coord, 0.5f);
}

void World::spawnChimneyAt(const glm::ivec3& coord, int height, int radius, double exitVelocity) {
    generator.generateChimneyAt(this, coord, height, radius);
    addChimneyEmitter(coord, static_cast<double>(height), exitVelocity, static_cast<double>(radius));
}

size_t World::HashChunkCoord::operator() (const glm::ivec2& coord) const noexcept {
    auto hashX = std::hash<int32_t>{}(coord.x);
    auto hashY = std::hash<int32_t>{}(coord.y);

    if (hashX != hashY) {
        return hashX ^ hashY;
    }

    return hashX;
}

World::RayCastResult World::raycastCheck(const glm::vec3& position, const glm::vec3& direction, bool discardFlora, bool discardWater) {
    RayCastResult result = {
        .face = { 0, 0, 0 }
    };
    
    float x = floor(position.x);
    float y = floor(position.y);
    float z = floor(position.z);
    float dx = direction.x;
    float dy = direction.y;
    float dz = direction.z;
    int32_t stepX = util::SignNum(dx);
    int32_t stepY = util::SignNum(dy);
    int32_t stepZ = util::SignNum(dz);

    float tMaxX = util::IntBound(position.x, dx);
    float tMaxY = util::IntBound(position.y, dy);
    float tMaxZ = util::IntBound(position.z, dz);

    float tDeltaX = stepX/dx;
    float tDeltaY = stepY/dy;
    float tDeltaZ = stepZ/dz;

    float radius = 500.0/sqrt(dx*dx+dy*dy+dz*dz);

    while (true) {
        yc::world::BlockData block = getBlockDataIfLoadedAt({ x, y, z });
        yc::world::BlockType blockType = block.getType();

        if (blockType == yc::world::BlockType::NONE) {
            result.block.setType(BlockType::NONE);
            break;
        } else if (blockType != yc::world::BlockType::AIR && 
            !(discardWater && blockType == yc::world::BlockType::WATER) &&
            !(discardFlora && block.isFlora())) {
            result.block = block;
            result.coord = { x, y, z };
            break;
        }

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                if (tMaxX > radius) { result.block.setType(BlockType::NONE); break; }
                // Update which cube we are now in.
                x += stepX;
                // Adjust tMaxX to the next X-oriented boundary crossing.
                tMaxX += tDeltaX;
                // Record the normal vector of the cube face we entered.
                result.face[0] = -stepX;
                result.face[1] = 0;
                result.face[2] = 0;
            } else {
                if (tMaxZ > radius) { result.block.setType(BlockType::NONE); break; }
                z += stepZ;
                tMaxZ += tDeltaZ;
                result.face[0] = 0;
                result.face[1] = 0;
                result.face[2] = -stepZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                if (tMaxY > radius) { result.block.setType(BlockType::NONE); break; }
                y += stepY;
                tMaxY += tDeltaY;
                result.face[0] = 0;
                result.face[1] = -stepY;
                result.face[2] = 0;
            } else {
                if (tMaxZ > radius) { result.block.setType(BlockType::NONE); break; }
                z += stepZ;
                tMaxZ += tDeltaZ;
                result.face[0] = 0;
                result.face[1] = 0;
                result.face[2] = -stepZ;
            }
        }
    }

    return result;
}

int32_t World::getSeed() const {
    return generator.getSeed();
}

void World::setSeed(int32_t seed) {
    generator.setSeed(seed);
}

void World::clearChunks() {
    chunks.clear();
    while (!shouldBeUnloadedChunks.empty()) {
        shouldBeUnloadedChunks.pop();
    }
}

void World::setChimneyEmitters(const std::vector<ChimneySource>& emitters) {
    chimneyEmitters = emitters;
}

void World::clearChimneyEmitters() {
    chimneyEmitters.clear();
}







}