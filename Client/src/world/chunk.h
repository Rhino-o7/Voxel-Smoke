#pragma once

#include "world/block.h"
#include "gl/mesh.h"
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>


namespace yc::world {

class World;

class Chunk : public std::enable_shared_from_this<Chunk> {

public:

    static const int32_t Width = 16;
    static const int32_t Length = 16;
    static const int32_t Height = 256;
    static const int32_t Volume = Width * Length * Height;

    Chunk();

    void setCoordinate(World* world, const glm::ivec2& coord);

    void buildMesh();

    void buildMeshIfNeeded();

    void prepareToBuildMesh();

    void renderOpaque();

    void renderTransparent();

    void renderFlora();

    static int32_t DistanceTo(const glm::ivec2& chunkCoord, const glm::ivec2& coord);

    BlockData getBlockDataAt(const glm::ivec3& coord);

    glm::ivec2 getCoord() const;

    glm::ivec3 getWorldCoordOfBlock(const glm::ivec3& blockCoord);

    glm::ivec3 getWorldCoord() const;

    BlockData* getChunkData();

    void setBlockData(const glm::ivec3& coord, BlockData blockData);

    void updateCropExposureBuffers(const std::function<float(const glm::ivec3&)>& exposureSampler);

    // CPU/GPU split build helpers for multithreaded generation.
    // Called from worker thread: generates vertex/index/exposure data without GL calls.
    void buildMeshCpu(std::shared_ptr<Chunk> northChunk = nullptr,
                      std::shared_ptr<Chunk> southChunk = nullptr,
                      std::shared_ptr<Chunk> eastChunk = nullptr,
                      std::shared_ptr<Chunk> westChunk = nullptr);

    // Called from main thread to upload staged mesh data to GL and create Mesh objects.
    void uploadMeshFromStaging();
    void startBackgroundBuild(std::shared_ptr<Chunk> self);

    bool tryMarkBuilding();
    void markNotBuilding();
    bool hasStagingData() const;
    bool isCurrentlyBuilding() const { return isBuilding.load(std::memory_order_acquire); }

private:
    struct CropMeshSpan {
        glm::ivec3 localCoord{};
        uint32_t start = 0;
        uint32_t count = 0;
    };

    BlockData blocks[Length][Height][Width];
    std::shared_ptr<yc::gl::Mesh> opaqueMesh;
    std::shared_ptr<yc::gl::Mesh> transparentMesh; 
    std::shared_ptr<yc::gl::Mesh> floraMesh;

    std::atomic<bool> needToBuildMesh{false};

    glm::ivec2 coord;
    World* world;
    std::unordered_map<uint32_t, CropMeshSpan> cropOpaqueMeshSpans;
    std::unordered_map<uint32_t, CropMeshSpan> cropTransparentMeshSpans;
    // Multithreading support
    std::atomic<bool> isBuilding{false};
    std::atomic<bool> stagingReady{false};
    std::mutex stagingMutex;
    // Protects access to the blocks array for concurrent CPU builds and main-thread writes.
    mutable std::mutex blocksMutex;

    struct MeshStagingData {
        std::vector<uint32_t> opaqueVertices;
        std::vector<uint32_t> opaqueIndices;
        std::vector<float> opaqueExposure;

        std::vector<uint32_t> transparentVertices;
        std::vector<uint32_t> transparentIndices;
        std::vector<float> transparentExposure;

        std::vector<float> floraVertexCoord;
        std::vector<float> floraVertexUv;
        std::vector<uint32_t> floraVertexTexCoord;
        std::vector<uint32_t> floraIndices;
        std::unordered_map<uint32_t, CropMeshSpan> cropOpaqueMeshSpans;
        std::unordered_map<uint32_t, CropMeshSpan> cropTransparentMeshSpans;
        bool builtOpaque = false;
        bool builtTransparent = false;
        bool builtFlora = false;
    } stagingData;
    
};

}