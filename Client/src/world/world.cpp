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
#include <sstream>
#include <array>
#include <algorithm>

namespace yc::world {

static float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

void World::addChimneyEmitter(const BlockPos& baseBlockCoord, double height, double exitVelocity, double radius) {
    ChimneySource src{};
    src.worldPos = getBlockToWorldCoord(baseBlockCoord);
    src.height = height;
    src.exitVelocity = exitVelocity;
    src.radius = radius;
    chimneyEmitters.push_back(src);
}

World::World(Persistence* persistence) : generator(0), persistence(persistence) {}

void World::init()
{
    // Start a small worker pool for background jobs
    const unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    stopWorkers.store(false);
    for (unsigned int i = 0; i < threadCount; ++i) {
        workerThreads.emplace_back([this]() {
            while (!stopWorkers.load()) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(this->jobQueueMutex);
                    this->jobQueueCv.wait_for(lock, std::chrono::milliseconds(50), [this]{ return !this->jobQueue.empty() || stopWorkers.load(); });
                    if (stopWorkers.load() && this->jobQueue.empty()) return;
                    if (this->jobQueue.empty()) continue;
                    job = std::move(this->jobQueue.front());
                    this->jobQueue.pop();
                }
                try {
                    job();
                } catch (...) {
                    // swallow exceptions from jobs
                }
            }
        });
    }
}

World::~World() {
    stopWorkers.store(true);
    jobQueueCv.notify_all();
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }
}

void World::enqueueJob(const std::function<void()>& job) {
    {
        std::lock_guard<std::mutex> lock(this->jobQueueMutex);
        jobQueue.push(job);
    }
    jobQueueCv.notify_one();
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

void World::renderOpaque(Camera* camera) {
    yc::Resource::GameTexure.bind();
    
    yc::Resource::OpaqueShader.use();
    yc::Resource::OpaqueShader.setMat4("projection_view", camera->getProjectionViewMatrix());
    yc::Resource::OpaqueShader.setFloat("uExposureScale", exposureScale);

    for (const auto& [coord, chunk]: this->chunks) {
        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(coord.x * Chunk::Length, 0, coord.y * Chunk::Width));
        yc::Resource::OpaqueShader.setMat4("model", model);
        chunk->renderOpaque();
    }
}

void World::renderTransparent(Camera* camera) {
    yc::Resource::GameTexure.bind();

    yc:: Resource::TransparentShader.use();
    yc:: Resource::TransparentShader.setMat4("projection_view", camera->getProjectionViewMatrix());
    yc:: Resource::TransparentShader.setFloat("uExposureScale", exposureScale);

    for (const auto& [coord, chunk]: this->chunks) {
        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(coord.x * Chunk::Length, 0, coord.y * Chunk::Width));
        yc:: Resource::TransparentShader.setMat4("model", model);
        chunk->renderTransparent();
    }
}

void World::renderFlora(yc::Camera* camera) {
    yc::Resource::GameTexure.bind();

    yc::Resource::FloraShader.use();
    yc::Resource::FloraShader.setMat4("projection_view", camera->getProjectionViewMatrix());

    for (const auto& [coord, chunk]: this->chunks) {
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