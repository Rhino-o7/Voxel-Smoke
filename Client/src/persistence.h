#pragma once

#include <string>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include "world/chunk.h"
#include "persistence_interface.h"

namespace yc {

struct Region {
    glm::ivec2 coord;
    int16_t offsets[1024];
    int16_t numGeneratedChunks;
    std::shared_ptr<std::fstream> file; // CHANGED: single stream for read/write
};

class Persistence : public yc::IPersistence {

public:
    explicit Persistence(const std::string& worldFolder = "save/");
    ~Persistence();

    void setWorldFolder(const std::string& folder);
    const std::string& getWorldFolder() const { return worldFolder; }
    void reset(const std::string& folder);

    void loadRegion(const glm::ivec2& regionCoord);
    void saveChunk(std::shared_ptr<yc::world::Chunk> chunk) override;
    void syncRegionFiles() override;
    std::string getRegionFileName(const glm::ivec2& regionCoord) const;
    std::shared_ptr<yc::world::Chunk> getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) override;

private:
    struct HashRegionCoord {
        size_t operator() (const glm::ivec2& coord) const noexcept;
    };

    bool ensureRegionFileOpen(const glm::ivec2& regionCoord, const std::shared_ptr<Region>& region);
    std::filesystem::path getWorldFolderPath() const;

    std::string worldFolder;
    std::unordered_map<glm::ivec2, std::shared_ptr<Region>, HashRegionCoord> regions;
};

}