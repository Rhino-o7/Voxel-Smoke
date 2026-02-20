#include "persistence.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <assert.h>
#include <filesystem>

namespace yc {

namespace fs = std::filesystem;

const size_t ChunkSize = yc::world::Chunk::Volume * sizeof(yc::world::BlockData);
const size_t RegionHeaderSize = sizeof(int16_t) + sizeof(int16_t) * 32 * 32;

static int32_t FloorDiv32(int32_t v) {
    return (v >= 0) ? (v / 32) : ((v + 1) / 32 - 1);
}

Persistence::Persistence(const std::string& folder) : worldFolder(folder) {}

Persistence::~Persistence() {
    for (auto& [regionCoord, region] : this->regions) {
        if (region->file && region->file->is_open()) {
            region->file->close();
        }
        region->file.reset();
    }
}

void Persistence::setWorldFolder(const std::string& folder) {
    worldFolder = folder;
}

void Persistence::reset(const std::string& folder) {
    for (auto& [regionCoord, region] : this->regions) {
        if (region->file && region->file->is_open()) {
            region->file->close();
        }
        region->file.reset();
    }
    regions.clear();
    worldFolder = folder;
}

fs::path Persistence::getWorldFolderPath() const {
    return fs::absolute(fs::path(worldFolder));
}

bool Persistence::ensureRegionFileOpen(const glm::ivec2& regionCoord, const std::shared_ptr<Region>& region) {
    if (!region) {
        return false;
    }

    if (region->file && region->file->is_open() && !region->file->fail()) {
        return true;
    }

    const fs::path worldFolderPath = getWorldFolderPath();
    fs::create_directories(worldFolderPath);

    const fs::path filePath = worldFolderPath / getRegionFileName(regionCoord);

    if (!fs::exists(filePath)) {
        std::ofstream newFile(filePath, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!newFile.is_open()) {
            std::cout << "ERROR: Failed to create region file: " << filePath.string() << "\n";
            return false;
        }

        newFile.write((char*)&region->numGeneratedChunks, sizeof(int16_t));
        newFile.write((char*)region->offsets, sizeof(region->offsets));
        newFile.flush();
        newFile.close();
    }

    region->file = std::make_shared<std::fstream>(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!region->file->is_open() || region->file->fail()) {
        std::error_code ec;
        const auto sz = fs::file_size(filePath, ec);

        std::cout
            << "ERROR: Failed to open region file: " << filePath.string() << "\n"
            << "  cwd: " << fs::current_path().string() << "\n"
            << "  exists: " << (fs::exists(filePath) ? "yes" : "no") << "\n"
            << "  size: " << (ec ? -1 : (int64_t)sz) << "\n";
        return false;
    }

    return true;
}

void Persistence::loadRegion(const glm::ivec2& regionCoord) {
    std::shared_ptr<Region> region = std::make_shared<Region>();
    region->coord = regionCoord;

    const std::string fileName = getRegionFileName(regionCoord);

    const fs::path worldFolderPath = getWorldFolderPath();
    fs::create_directories(worldFolderPath); // IMPORTANT: ensure save dir exists

    const fs::path filePath = worldFolderPath / fileName;

    // Create the file if missing
    if (!fs::exists(filePath)) {
        for (int i = 0; i < 32 * 32; ++i) region->offsets[i] = -1;
        region->numGeneratedChunks = 0;

        std::ofstream newFile(filePath, std::ios::binary | std::ios::out | std::ios::trunc);
        newFile.write((char*)&region->numGeneratedChunks, sizeof(int16_t));
        newFile.write((char*)region->offsets, sizeof(region->offsets));
        newFile.flush();
        newFile.close();

        std::cout << "Created new region file " << filePath.string() << "\n";
    }

    region->file = std::make_shared<std::fstream>(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!region->file->is_open() || region->file->fail()) {
        std::error_code ec;
        const auto sz = fs::file_size(filePath, ec);

        std::cout
            << "ERROR: Failed to open region file: " << filePath.string() << "\n"
            << "  cwd: " << fs::current_path().string() << "\n"
            << "  exists: " << (fs::exists(filePath) ? "yes" : "no") << "\n"
            << "  size: " << (ec ? -1 : (int64_t)sz) << "\n";
        return;
    }

    // Read header
    region->file->clear();
    region->file->seekg(0, std::ios::beg);
    region->file->read((char*)&region->numGeneratedChunks, sizeof(int16_t));
    region->file->seekg(sizeof(int16_t), std::ios::beg);
    region->file->read((char*)&region->offsets[0], sizeof(region->offsets));

    this->regions[regionCoord] = region;
}

std::shared_ptr<yc::world::Chunk> Persistence::getChunk(const glm::ivec2& chunkCoord, yc::world::World* world) {
    const int32_t regionX = FloorDiv32(chunkCoord.x);
    const int32_t regionY = FloorDiv32(chunkCoord.y);

    const int32_t localX = chunkCoord.x - regionX * 32;
    const int32_t localY = chunkCoord.y - regionY * 32;

    const glm::ivec2 regionCoord = { regionX, regionY };
    const int16_t chunkId = static_cast<int16_t>(localX + localY * 32);

    if (this->regions.find(regionCoord) == this->regions.end()) {
        loadRegion(regionCoord);
    }

    const auto& region = this->regions[regionCoord];
    if (!ensureRegionFileOpen(regionCoord, region)) {
        return nullptr;
    }

    if (region->offsets[chunkId] == -1) {
        return nullptr;
    }

    auto chunk = std::make_shared<yc::world::Chunk>();
    chunk->setCoordinate(world, chunkCoord);

    region->file->clear();
    region->file->seekg(RegionHeaderSize + ChunkSize * region->offsets[chunkId], std::ios::beg);
    region->file->read((char*)chunk->getChunkData(), ChunkSize);

    return chunk;
}

void Persistence::saveChunk(std::shared_ptr<yc::world::Chunk> chunk) {
    const glm::ivec2 chunkCoord = chunk->getCoord();

    const int32_t regionX = FloorDiv32(chunkCoord.x);
    const int32_t regionY = FloorDiv32(chunkCoord.y);

    const int32_t localX = chunkCoord.x - regionX * 32;
    const int32_t localY = chunkCoord.y - regionY * 32;

    const glm::ivec2 regionCoord = { regionX, regionY };
    const int16_t chunkId = static_cast<int16_t>(localX + localY * 32);

    if (this->regions.find(regionCoord) == this->regions.end()) {
        loadRegion(regionCoord);
    }

    const auto& region = this->regions[regionCoord];
    if (!ensureRegionFileOpen(regionCoord, region)) {
        std::cout << "ERROR: Region not open for save: " << (worldFolder + getRegionFileName(regionCoord)) << "\n";
        return;
    }

    // Allocate offset slot if first time saving this chunk
    if (region->offsets[chunkId] == -1) {
        region->offsets[chunkId] = region->numGeneratedChunks;

        // Persist offsets[chunkId]
        region->file->clear();
        region->file->seekp(sizeof(int16_t) + chunkId * sizeof(int16_t), std::ios::beg);
        region->file->write((char*)&region->offsets[chunkId], sizeof(int16_t));

        // Persist numGeneratedChunks
        region->numGeneratedChunks += 1;
        region->file->seekp(0, std::ios::beg);
        region->file->write((char*)&region->numGeneratedChunks, sizeof(int16_t));

        region->file->flush(); // IMPORTANT: header must hit disk
    }

    // Write chunk payload
    region->file->clear();
    region->file->seekp(RegionHeaderSize + ChunkSize * region->offsets[chunkId], std::ios::beg);
    region->file->write((char*)chunk->getChunkData(), ChunkSize);
    region->file->flush();
}

void Persistence::syncRegionFiles() {
    for (auto& [regionCoord, region] : this->regions) {
        if (region && region->file && region->file->is_open()) {
            region->file->flush();
        }
    }
}

std::string Persistence::getRegionFileName(const glm::ivec2& regionCoord) const {
    std::stringstream fileName;
    fileName << "r." << regionCoord.x << "." << regionCoord.y << ".cjbt";
    return fileName.str();
}

size_t Persistence::HashRegionCoord::operator() (const glm::ivec2& coord) const noexcept {
    auto hashX = std::hash<int32_t>{}(coord.x);
    auto hashY = std::hash<int32_t>{}(coord.y);

    if (hashX != hashY) {
        return hashX ^ hashY;
    }

    return hashX;
}

}