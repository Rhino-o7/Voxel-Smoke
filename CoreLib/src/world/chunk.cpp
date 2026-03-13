#include <array>
#include "world/chunk.h"
#include "world/world.h"
#include "graphic/block_vertex.h"
#include <iostream>
#include <random>
#include "graphic/flora_vertex.h"

namespace yc::world {

const std::array<glm::ivec3, 6> directionsToCheck = {{
    { 0, 0, +1 }, // front
    { 0, 0, -1 }, // back
    { +1, 0, 0 }, // right
    { -1, 0, 0 }, // left
    { 0, +1, 0 }, // top
    { 0, -1, 0 }, // bottom
}};

static uint32_t PackLocalCoord(const glm::ivec3& coord) {
    return (coord.x & 0x1F)
        | ((coord.y & 0x1FF) << 5)
        | ((coord.z & 0x1F) << 14);
}

void Chunk::buildMeshCpu(std::shared_ptr<Chunk> northChunk,
                         std::shared_ptr<Chunk> southChunk,
                         std::shared_ptr<Chunk> eastChunk,
                         std::shared_ptr<Chunk> westChunk) {
    // Build CPU-side staging buffers first; GPU upload happens later on main thread.
    MeshStagingData localStaging;

    size_t chunkOpaqueVerticesSize = 0;
    size_t chunkOpaqueIndicesSize = 0;
    size_t chunkTransparentVerticesSize = 0;
    size_t chunkTransparentIndicesSize = 0;

    // Reserve to reduce reallocations
    localStaging.opaqueVertices.reserve(65536);
    localStaging.opaqueIndices.reserve(131072);
    localStaging.opaqueExposure.reserve(65536);
    localStaging.transparentVertices.reserve(65536);
    localStaging.transparentIndices.reserve(131072);
    localStaging.transparentExposure.reserve(65536);
    localStaging.floraVertexCoord.reserve(4096);
    localStaging.floraVertexUv.reserve(4096);
    localStaging.floraVertexTexCoord.reserve(2048);
    localStaging.floraIndices.reserve(4096);

    // Use provided neighbor shared_ptrs or fallback to querying world
    if (!northChunk && this->world) northChunk = this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y - 1 });
    if (!southChunk && this->world) southChunk = this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y + 1 });
    if (!eastChunk  && this->world) eastChunk  = this->world->getChunkIfLoadedAt({ this->coord.x + 1, this->coord.y });
    if (!westChunk  && this->world) westChunk  = this->world->getChunkIfLoadedAt({ this->coord.x - 1, this->coord.y });

    const BlockData defaultAir{ BlockType::AIR };

    std::lock_guard<std::mutex> readLock(this->blocksMutex);

    auto getNeighborBlock = [&](int32_t x, int32_t y, int32_t z) -> const BlockData* {
        // Resolve neighbor blocks across chunk boundaries using loaded neighbors when available.
        if (y < 0 || y >= Chunk::Height) {
            return nullptr;
        }

        if (x >= 0 && x < Chunk::Length && z >= 0 && z < Chunk::Width) {
            return &this->blocks[x][y][z];
        }

        if (x < 0) {
            return (westChunk != nullptr) ? &westChunk->blocks[Chunk::Length - 1][y][z] : &defaultAir;
        }

        if (x >= Chunk::Length) {
            return (eastChunk != nullptr) ? &eastChunk->blocks[0][y][z] : &defaultAir;
        }

        if (z < 0) {
            return (northChunk != nullptr) ? &northChunk->blocks[x][y][Chunk::Width - 1] : &defaultAir;
        }

        if (z >= Chunk::Width) {
            return (southChunk != nullptr) ? &southChunk->blocks[x][y][0] : &defaultAir;
        }

        return &defaultAir;
    };

    auto shouldRenderFace = [&](BlockType blockType, const BlockData* neighborBlock) {
        // Skip faces hidden by opaque neighbors or merged transparent faces of same type.
        if (neighborBlock == nullptr) {
            return true;
        }

        const BlockData& blockToCheck = *neighborBlock;

        if (blockType == blockToCheck.getType() && blockToCheck.isTransparent() && blockType != BlockType::LEAF) {
            return false;
        }

        if (blockToCheck.isOpaque() && blockType != BlockType::GLASS) {
            return false;
        }

        return true;
    };

    auto appendOpaqueVertex = [&](int32_t x, int32_t y, int32_t z, uint32_t uvX, uint32_t uvY, BlockType blockType, const glm::ivec3& direction) {
        yc::graphic::BlockVertex vertex({ static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z) }, { uvX, uvY });
        vertex.setBlockType(blockType, direction);
        localStaging.opaqueVertices.push_back(vertex.getData());
        localStaging.opaqueExposure.push_back(0.0f);
        ++chunkOpaqueVerticesSize;
    };

    auto appendOpaqueQuad = [&](int32_t faceIndex,
                                BlockType blockType,
                                int32_t x0, int32_t x1,
                                int32_t y0, int32_t y1,
                                int32_t z0, int32_t z1) {
        const uint32_t opaqueId = static_cast<uint32_t>(chunkOpaqueVerticesSize);
        const glm::ivec3 direction = directionsToCheck[faceIndex];

        switch (faceIndex) {
            case 0: // front
                appendOpaqueVertex(x1, y0, z0, 1, 0, blockType, direction);
                appendOpaqueVertex(x1, y1, z0, 1, 1, blockType, direction);
                appendOpaqueVertex(x0, y1, z0, 0, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z0, 0, 0, blockType, direction);
                break;

            case 1: // back
                appendOpaqueVertex(x1, y0, z0, 0, 0, blockType, direction);
                appendOpaqueVertex(x1, y1, z0, 0, 1, blockType, direction);
                appendOpaqueVertex(x0, y1, z0, 1, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z0, 1, 0, blockType, direction);
                break;

            case 2: // right
                appendOpaqueVertex(x0, y0, z0, 1, 0, blockType, direction);
                appendOpaqueVertex(x0, y1, z0, 1, 1, blockType, direction);
                appendOpaqueVertex(x0, y1, z1, 0, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z1, 0, 0, blockType, direction);
                break;

            case 3: // left
                appendOpaqueVertex(x0, y0, z0, 0, 0, blockType, direction);
                appendOpaqueVertex(x0, y1, z0, 0, 1, blockType, direction);
                appendOpaqueVertex(x0, y1, z1, 1, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z1, 1, 0, blockType, direction);
                break;

            case 4: // top
                appendOpaqueVertex(x1, y0, z0, 1, 1, blockType, direction);
                appendOpaqueVertex(x1, y0, z1, 1, 0, blockType, direction);
                appendOpaqueVertex(x0, y0, z1, 0, 0, blockType, direction);
                appendOpaqueVertex(x0, y0, z0, 0, 1, blockType, direction);
                break;

            case 5: // bottom
            default:
                appendOpaqueVertex(x1, y0, z1, 0, 0, blockType, direction);
                appendOpaqueVertex(x1, y0, z0, 0, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z0, 1, 1, blockType, direction);
                appendOpaqueVertex(x0, y0, z1, 1, 0, blockType, direction);
                break;
        }

        localStaging.opaqueIndices.push_back(opaqueId + 0);
        localStaging.opaqueIndices.push_back(opaqueId + 1);
        localStaging.opaqueIndices.push_back(opaqueId + 2);
        localStaging.opaqueIndices.push_back(opaqueId + 0);
        localStaging.opaqueIndices.push_back(opaqueId + 2);
        localStaging.opaqueIndices.push_back(opaqueId + 3);
        chunkOpaqueIndicesSize += 6;
    };

    // Transparent blocks and flora keep per-block faces to preserve blending/exposure behavior.
    for (int32_t x = 0; x < Chunk::Length; ++x)
    for (int32_t y = 0; y < Chunk::Height; ++y)
    for (int32_t z = 0; z < Chunk::Width; ++z) {
        const BlockData block = this->blocks[x][y][z];
        const BlockType blockType = block.getType();

        if (block.isFlora()) {
            const uint32_t id = static_cast<uint32_t>(localStaging.floraVertexCoord.size() / 3);
            for (int i = 0; i < 8; ++i) {
                const float vx = x + yc::graphic::FloraVertices[i][0];
                const float vy = y + yc::graphic::FloraVertices[i][1];
                const float vz = z + yc::graphic::FloraVertices[i][2];
                const float uvX = yc::graphic::FloraTexcoords[i][0];
                const float uvY = yc::graphic::FloraTexcoords[i][1];
                const uint32_t texCoord = yc::graphic::GetFloraTexureCoord(blockType, (i < 4) ? 1 : 2);

                localStaging.floraVertexCoord.push_back(vx);
                localStaging.floraVertexCoord.push_back(vy);
                localStaging.floraVertexCoord.push_back(vz);
                localStaging.floraVertexUv.push_back(uvX);
                localStaging.floraVertexUv.push_back(uvY);
                localStaging.floraVertexTexCoord.push_back(texCoord);
            }
            localStaging.floraIndices.push_back(id + 0);
            localStaging.floraIndices.push_back(id + 1);
            localStaging.floraIndices.push_back(id + 2);
            localStaging.floraIndices.push_back(id + 0);
            localStaging.floraIndices.push_back(id + 2);
            localStaging.floraIndices.push_back(id + 3);
            localStaging.floraIndices.push_back(id + 4);
            localStaging.floraIndices.push_back(id + 5);
            localStaging.floraIndices.push_back(id + 6);
            localStaging.floraIndices.push_back(id + 4);
            localStaging.floraIndices.push_back(id + 6);
            localStaging.floraIndices.push_back(id + 7);
            continue;
        }

        if (blockType == BlockType::AIR || block.isOpaque()) {
            continue;
        }

        float exposure = 0.0f;
        if (blockType == BlockType::CROP && world != nullptr) {
            const glm::ivec3 worldBlock = getWorldCoordOfBlock({ x, y, z });
            exposure = static_cast<float>(world->getCropExposureAtBlock(worldBlock));
        }

        const bool trackCrop = (blockType == BlockType::CROP);
        const uint32_t cropTransparentStart = static_cast<uint32_t>(chunkTransparentVerticesSize);

        for (const auto& direction : directionsToCheck) {
            const BlockData* neighborBlock = getNeighborBlock(x + direction.x, y + direction.y, z + direction.z);
            if (!shouldRenderFace(blockType, neighborBlock)) {
                continue;
            }

            const uint32_t transparentId = static_cast<uint32_t>(chunkTransparentVerticesSize);
            const auto& baseVertices = yc::graphic::BlockVertex::GetVerticesFromDirection(direction);

            for (size_t vi = 0; vi < 4; ++vi) {
                yc::graphic::BlockVertex vertex = baseVertices[vi];
                vertex.moveCoordinate(x, y, z);
                vertex.setBlockType(blockType, direction);
                localStaging.transparentVertices.push_back(vertex.getData());
                localStaging.transparentExposure.push_back(exposure);
                ++chunkTransparentVerticesSize;
            }

            localStaging.transparentIndices.push_back(transparentId + 0);
            localStaging.transparentIndices.push_back(transparentId + 1);
            localStaging.transparentIndices.push_back(transparentId + 2);
            localStaging.transparentIndices.push_back(transparentId + 0);
            localStaging.transparentIndices.push_back(transparentId + 2);
            localStaging.transparentIndices.push_back(transparentId + 3);
            chunkTransparentIndicesSize += 6;
        }

        if (trackCrop) {
            const uint32_t count = static_cast<uint32_t>(chunkTransparentVerticesSize) - cropTransparentStart;
            localStaging.cropTransparentMeshSpans[PackLocalCoord({ x, y, z })] = { { x, y, z }, cropTransparentStart, count };
        }
    }

    // Greedy meshing for opaque blocks.
    struct OpaqueMaskCell {
        bool valid = false;
        BlockType type = BlockType::AIR;
        BlockFaceDirection faceDirection = BlockFaceDirection::NONE;
    };

    std::array<OpaqueMaskCell, Chunk::Height * Chunk::Width> mask{};

    for (int32_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const glm::ivec3 direction = directionsToCheck[faceIndex];
        const int32_t d = (direction.x != 0) ? 0 : ((direction.y != 0) ? 1 : 2);
        const int32_t u = (d == 0) ? 1 : 0;
        const int32_t v = (d == 2) ? 1 : 2;

        const int32_t dimD = (d == 0) ? Chunk::Length : ((d == 1) ? Chunk::Height : Chunk::Width);
        const int32_t dimU = (u == 0) ? Chunk::Length : ((u == 1) ? Chunk::Height : Chunk::Width);
        const int32_t dimV = (v == 0) ? Chunk::Length : ((v == 1) ? Chunk::Height : Chunk::Width);

        for (int32_t p = 0; p < dimD; ++p) {
            for (int32_t j = 0; j < dimV; ++j) {
                for (int32_t i = 0; i < dimU; ++i) {
                    OpaqueMaskCell& cell = mask[j * dimU + i];
                    cell.valid = false;

                    std::array<int32_t, 3> blockCoord{};
                    blockCoord[d] = p;
                    blockCoord[u] = i;
                    blockCoord[v] = j;

                    const BlockData block = this->blocks[blockCoord[0]][blockCoord[1]][blockCoord[2]];
                    if (!block.isOpaque()) {
                        continue;
                    }

                    const BlockType blockType = block.getType();
                    const BlockData* neighborBlock = getNeighborBlock(
                        blockCoord[0] + direction.x,
                        blockCoord[1] + direction.y,
                        blockCoord[2] + direction.z);

                    if (!shouldRenderFace(blockType, neighborBlock)) {
                        continue;
                    }

                    cell.valid = true;
                    cell.type = blockType;
                    cell.faceDirection = block.getFaceDirection();
                }
            }

            for (int32_t j = 0; j < dimV; ++j) {
                for (int32_t i = 0; i < dimU;) {
                    OpaqueMaskCell& base = mask[j * dimU + i];
                    if (!base.valid) {
                        ++i;
                        continue;
                    }

                    int32_t width = 1;
                    while (i + width < dimU) {
                        OpaqueMaskCell& next = mask[j * dimU + (i + width)];
                        if (!next.valid || next.type != base.type || next.faceDirection != base.faceDirection) {
                            break;
                        }
                        ++width;
                    }

                    int32_t height = 1;
                    bool canGrow = true;
                    while (j + height < dimV && canGrow) {
                        for (int32_t k = 0; k < width; ++k) {
                            OpaqueMaskCell& next = mask[(j + height) * dimU + (i + k)];
                            if (!next.valid || next.type != base.type || next.faceDirection != base.faceDirection) {
                                canGrow = false;
                                break;
                            }
                        }

                        if (canGrow) {
                            ++height;
                        }
                    }

                    std::array<int32_t, 3> minCoord{};
                    std::array<int32_t, 3> maxCoord{};

                    const int32_t plane = (direction[d] > 0) ? (p + 1) : p;
                    minCoord[d] = plane;
                    maxCoord[d] = plane;
                    minCoord[u] = i;
                    maxCoord[u] = i + width;
                    minCoord[v] = j;
                    maxCoord[v] = j + height;

                    appendOpaqueQuad(
                        faceIndex,
                        base.type,
                        minCoord[0], maxCoord[0],
                        minCoord[1], maxCoord[1],
                        minCoord[2], maxCoord[2]);

                    for (int32_t y = 0; y < height; ++y) {
                        for (int32_t x = 0; x < width; ++x) {
                            mask[(j + y) * dimU + (i + x)].valid = false;
                        }
                    }

                    i += width;
                }
            }
        }
    }

    // Commit local staging into the chunk's stagingData under lock
    {
        std::lock_guard<std::mutex> guard(this->stagingMutex);
        stagingData.opaqueVertices.swap(localStaging.opaqueVertices);
        stagingData.opaqueIndices.swap(localStaging.opaqueIndices);
        stagingData.opaqueExposure.swap(localStaging.opaqueExposure);

        stagingData.transparentVertices.swap(localStaging.transparentVertices);
        stagingData.transparentIndices.swap(localStaging.transparentIndices);
        stagingData.transparentExposure.swap(localStaging.transparentExposure);

        stagingData.floraVertexCoord.swap(localStaging.floraVertexCoord);
        stagingData.floraVertexUv.swap(localStaging.floraVertexUv);
        stagingData.floraVertexTexCoord.swap(localStaging.floraVertexTexCoord);
        stagingData.floraIndices.swap(localStaging.floraIndices);

        // copy crop spans into staging as well so upload pass can update exposure ranges
        stagingData.cropOpaqueMeshSpans.swap(localStaging.cropOpaqueMeshSpans);
        stagingData.cropTransparentMeshSpans.swap(localStaging.cropTransparentMeshSpans);
    }

    // Make sure main thread knows staging is ready for upload
    stagingReady.store(true, std::memory_order_release);
}

void Chunk::uploadMeshFromStaging() {
    std::lock_guard<std::mutex> guard(this->stagingMutex);
    // Transfer prepared staging buffers into GPU meshes.

    // Opaque
    if (stagingData.opaqueIndices.empty()) {
        opaqueMesh.reset();
    } else if (opaqueMesh != nullptr) {
        opaqueMesh->bind();
        opaqueMesh->updateIndices(stagingData.opaqueIndices.data(), stagingData.opaqueIndices.size());
        opaqueMesh->updateStaticBuffer(0, stagingData.opaqueVertices.data(), stagingData.opaqueVertices.size());
        opaqueMesh->updateStaticBuffer(1, stagingData.opaqueExposure.data(), stagingData.opaqueExposure.size());
    } else {
        opaqueMesh = std::make_shared<yc::gl::Mesh>();
        opaqueMesh->init();
        opaqueMesh->bind();
        opaqueMesh->addIndices(stagingData.opaqueIndices.data(), stagingData.opaqueIndices.size());
        opaqueMesh->addStaticBuffer(1, stagingData.opaqueVertices.data(), stagingData.opaqueVertices.size());
        opaqueMesh->addStaticBuffer(1, stagingData.opaqueExposure.data(), stagingData.opaqueExposure.size());
    }

    // Transparent
    if (stagingData.transparentIndices.empty()) {
        transparentMesh.reset();
    } else if (transparentMesh != nullptr) {
        transparentMesh->bind();
        transparentMesh->updateIndices(stagingData.transparentIndices.data(), stagingData.transparentIndices.size());
        transparentMesh->updateStaticBuffer(0, stagingData.transparentVertices.data(), stagingData.transparentVertices.size());
        transparentMesh->updateStaticBuffer(1, stagingData.transparentExposure.data(), stagingData.transparentExposure.size());
    } else {
        transparentMesh = std::make_shared<yc::gl::Mesh>();
        transparentMesh->init();
        transparentMesh->bind();
        transparentMesh->addIndices(stagingData.transparentIndices.data(), stagingData.transparentIndices.size());
        transparentMesh->addStaticBuffer(1, stagingData.transparentVertices.data(), stagingData.transparentVertices.size());
        transparentMesh->addStaticBuffer(1, stagingData.transparentExposure.data(), stagingData.transparentExposure.size());
    }

    // Flora
    if (stagingData.floraIndices.empty()) {
        floraMesh.reset();
    } else if (floraMesh != nullptr) {
        floraMesh->bind();
        floraMesh->updateIndices(stagingData.floraIndices);
        floraMesh->updateStaticBuffer(0, stagingData.floraVertexCoord);
        floraMesh->updateStaticBuffer(1, stagingData.floraVertexUv);
        floraMesh->updateStaticBuffer(2, stagingData.floraVertexTexCoord);
    } else {
        floraMesh = std::make_shared<yc::gl::Mesh>();
        floraMesh->init();
        floraMesh->bind();
        floraMesh->addIndices(stagingData.floraIndices);
        floraMesh->addStaticBuffer(3, stagingData.floraVertexCoord);
        floraMesh->addStaticBuffer(2, stagingData.floraVertexUv);
        floraMesh->addStaticBuffer(1, stagingData.floraVertexTexCoord);
    }

    // Clear staging vectors to free memory
    MeshStagingData empty;
    stagingData.opaqueVertices.swap(empty.opaqueVertices);
    stagingData.opaqueIndices.swap(empty.opaqueIndices);
    stagingData.opaqueExposure.swap(empty.opaqueExposure);
    stagingData.transparentVertices.swap(empty.transparentVertices);
    stagingData.transparentIndices.swap(empty.transparentIndices);
    stagingData.transparentExposure.swap(empty.transparentExposure);
    stagingData.floraVertexCoord.swap(empty.floraVertexCoord);
    stagingData.floraVertexUv.swap(empty.floraVertexUv);
    stagingData.floraVertexTexCoord.swap(empty.floraVertexTexCoord);
    stagingData.floraIndices.swap(empty.floraIndices);
    stagingData.cropOpaqueMeshSpans.clear();
    stagingData.cropTransparentMeshSpans.clear();

    // Mark staging consumed
    stagingReady.store(false, std::memory_order_release);

    this->needToBuildMesh = false;
}

bool Chunk::tryMarkBuilding() {
    bool expected = false;
    return isBuilding.compare_exchange_strong(expected, true);
}

void Chunk::markNotBuilding() {
    isBuilding.store(false);
}

bool Chunk::hasStagingData() const {
    return stagingReady.load(std::memory_order_acquire);
}

Chunk::Chunk():
    needToBuildMesh(false),
    floraMesh(nullptr),
    opaqueMesh(nullptr),
    transparentMesh(nullptr) {
}

BlockData* Chunk::getChunkData() {
    return &this->blocks[0][0][0];
}

void Chunk::prepareToBuildMesh() {
    this->needToBuildMesh.store(true, std::memory_order_release);
}

void Chunk::setCoordinate(World* world, const glm::ivec2& coord) {
    this->world = world;
    this->coord = coord;
}

glm::ivec3 Chunk::getWorldCoord() const {
    return { this->coord.x * Chunk::Length, 0, this->coord.y * Chunk::Width };
}

void Chunk::buildMeshIfNeeded() {
    if (this->needToBuildMesh.load(std::memory_order_acquire)) {
        // Try to start a background CPU build if not already building.
        // If any horizontal neighbour chunk is not yet loaded, postpone build
        // to avoid producing border faces that will be immediately invalidated.
        if (this->world) {
            const glm::ivec2 n = { this->coord.x, this->coord.y - 1 };
            const glm::ivec2 s = { this->coord.x, this->coord.y + 1 };
            const glm::ivec2 e = { this->coord.x + 1, this->coord.y };
            const glm::ivec2 w = { this->coord.x - 1, this->coord.y };
            if (!this->world->isChunkLoaded(n) || !this->world->isChunkLoaded(s) || !this->world->isChunkLoaded(e) || !this->world->isChunkLoaded(w)) {
                // Keep needToBuildMesh true so the build will be attempted again when neighbours load.
                return;
            }
        }

        if (tryMarkBuilding()) {
            // Queue CPU mesh generation on worker threads to keep frame thread responsive.
            // Use shared_from_this to keep chunk alive while worker runs
            std::shared_ptr<Chunk> self = this->shared_from_this();
            if (self && this->world) {
                auto north = this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y - 1 });
                auto south = this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y + 1 });
                auto east  = this->world->getChunkIfLoadedAt({ this->coord.x + 1, this->coord.y });
                auto west  = this->world->getChunkIfLoadedAt({ this->coord.x - 1, this->coord.y });

                this->world->enqueueJob([self, north, south, east, west]() {
                    self->buildMeshCpu(north, south, east, west);
                    // ensure building flag is cleared after CPU work completes
                    self->markNotBuilding();
                });
            } else {
                // fallback synchronous
                this->buildMeshCpu(nullptr, nullptr, nullptr, nullptr);
                this->stagingReady.store(true, std::memory_order_release);
                this->markNotBuilding();
            }
        } else if (stagingReady.load(std::memory_order_acquire)) {
            // If staging data exists (built by worker), upload it to GPU now.
            uploadMeshFromStaging();
            stagingReady.store(false, std::memory_order_release);
        }
    }
}

void Chunk::buildMesh() {
    auto northChunk = this->world ? this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y - 1 }) : nullptr;
    auto southChunk = this->world ? this->world->getChunkIfLoadedAt({ this->coord.x, this->coord.y + 1 }) : nullptr;
    auto eastChunk  = this->world ? this->world->getChunkIfLoadedAt({ this->coord.x + 1, this->coord.y }) : nullptr;
    auto westChunk  = this->world ? this->world->getChunkIfLoadedAt({ this->coord.x - 1, this->coord.y }) : nullptr;

    buildMeshCpu(northChunk, southChunk, eastChunk, westChunk);
    uploadMeshFromStaging();
}

glm::ivec2 Chunk::getCoord() const {
    return this->coord;
}

glm::ivec3 Chunk::getWorldCoordOfBlock(const glm::ivec3& blockCoord) {
    assert(blockCoord.x>=0 && blockCoord.x<Chunk::Length);
    assert(blockCoord.y>=0 && blockCoord.y<Chunk::Height);
    assert(blockCoord.z>=0 && blockCoord.z<Chunk::Width);

    return {
        blockCoord.x + (this->coord.x * Chunk::Length),
        blockCoord.y,
        blockCoord.z + (this->coord.y * Chunk::Width)
    };
}

BlockData Chunk::getBlockDataAt(const glm::ivec3& coord) {
    assert(coord.x>=0 && coord.x<Chunk::Length);
    assert(coord.y>=0 && coord.y<Chunk::Height);
    assert(coord.z>=0 && coord.z<Chunk::Width);

    std::lock_guard<std::mutex> rlock(this->blocksMutex);
    return this->blocks[coord.x][coord.y][coord.z];
}

void Chunk::renderOpaque() {
    if (opaqueMesh == nullptr) return;
    opaqueMesh->bind();
    opaqueMesh->draw();
}

void Chunk::renderTransparent() {
    if (transparentMesh == nullptr) return;
    transparentMesh->bind();
    transparentMesh->draw();
}

void Chunk::renderFlora() {
    if (floraMesh == nullptr) return;
    floraMesh->bind();
    floraMesh->draw();
}

void Chunk::setBlockData(const glm::ivec3& coord, BlockData blockData) {
    std::lock_guard<std::mutex> wlock(this->blocksMutex);
    this->blocks[coord.x][coord.y][coord.z] = blockData;
}

int32_t Chunk::DistanceTo(const glm::ivec2& chunkCoord, const glm::ivec2& coord) {
    
    return sqrt(
        (chunkCoord.x-coord.x)
        *(chunkCoord.x-coord.x)
        + (chunkCoord.y-coord.y)
        *(chunkCoord.y-coord.y));
}

void Chunk::updateCropExposureBuffers(const std::function<float(const glm::ivec3&)>& exposureSampler) {
    static std::vector<float> exposureData;

    if (opaqueMesh && !cropOpaqueMeshSpans.empty()) {
        opaqueMesh->bind();
        for (const auto& [key, span] : cropOpaqueMeshSpans) {
            if (span.count == 0) continue;
            const glm::ivec3 worldCoord = getWorldCoordOfBlock(span.localCoord);
            const float exposure = exposureSampler(worldCoord);
            exposureData.assign(span.count, exposure);
            opaqueMesh->updateStaticBufferRange(1, span.start, exposureData.data(), span.count);
        }
    }

    if (transparentMesh && !cropTransparentMeshSpans.empty()) {
        transparentMesh->bind();
        for (const auto& [key, span] : cropTransparentMeshSpans) {
            if (span.count == 0) continue;
            const glm::ivec3 worldCoord = getWorldCoordOfBlock(span.localCoord);
            const float exposure = exposureSampler(worldCoord);
            exposureData.assign(span.count, exposure);
            transparentMesh->updateStaticBufferRange(1, span.start, exposureData.data(), span.count);
        }
    }
}

}