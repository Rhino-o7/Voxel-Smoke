#pragma once

#include "world/block.h"
#include "gl/mesh.h"
#include <unordered_map>
#include <functional>


namespace yc::world {

class World;

class Chunk {

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

    bool needToBuildMesh;

    glm::ivec2 coord;
    World* world;
    std::unordered_map<uint32_t, CropMeshSpan> cropOpaqueMeshSpans;
    std::unordered_map<uint32_t, CropMeshSpan> cropTransparentMeshSpans;
    
};

}