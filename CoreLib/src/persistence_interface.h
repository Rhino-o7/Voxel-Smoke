#pragma once

#include <memory>

#include <glm/glm.hpp>

namespace yc {
namespace world {
class World;
class Chunk;
}

class IPersistence {
public:
    virtual ~IPersistence() = default;

    virtual void saveChunk(std::shared_ptr<yc::world::Chunk> chunk) = 0;
    virtual void syncRegionFiles() = 0;
    virtual std::shared_ptr<yc::world::Chunk> getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) = 0;
};

}
