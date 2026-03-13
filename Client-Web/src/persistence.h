#pragma once

#include <string>

#include "persistence_interface.h"
#include "world/chunk.h"

namespace yc {

class Persistence : public yc::IPersistence {

public:
    explicit Persistence(const std::string& worldFolder = "save/");
    ~Persistence();

    void setWorldFolder(const std::string& folder);
    const std::string& getWorldFolder() const { return worldFolder; }
    void reset(const std::string& folder);

    void saveChunk(std::shared_ptr<yc::world::Chunk> chunk) override;
    void syncRegionFiles() override;
    std::shared_ptr<yc::world::Chunk> getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) override;

private:
    std::shared_ptr<yc::world::Chunk> createChunkFromBytes(const glm::ivec2& chunkCoord, yc::world::World* world, const std::string& bytes) const;

    std::string worldFolder;
};

}