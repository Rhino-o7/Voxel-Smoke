#include "save_system.h"

#include <fstream>
#include <vector>
#include <algorithm>

namespace yc {

namespace fs = std::filesystem;

namespace {
    constexpr uint32_t SaveMagic = 0x56534359; // "YCSV"
    constexpr uint32_t SaveVersion = 1;

    struct SaveHeader {
        uint32_t magic = SaveMagic;
        uint32_t version = SaveVersion;
    };

    bool WriteBytes(std::ofstream& out, const void* data, size_t size) {
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        return out.good();
    }

    bool ReadBytes(std::ifstream& in, void* data, size_t size) {
        in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
        return in.good();
    }

    bool WriteU32(std::ofstream& out, uint32_t value) { return WriteBytes(out, &value, sizeof(value)); }
    bool ReadU32(std::ifstream& in, uint32_t& value) { return ReadBytes(in, &value, sizeof(value)); }

    bool WriteU8(std::ofstream& out, uint8_t value) { return WriteBytes(out, &value, sizeof(value)); }
    bool ReadU8(std::ifstream& in, uint8_t& value) { return ReadBytes(in, &value, sizeof(value)); }

    bool WriteDouble(std::ofstream& out, double value) { return WriteBytes(out, &value, sizeof(value)); }
    bool ReadDouble(std::ifstream& in, double& value) { return ReadBytes(in, &value, sizeof(value)); }

    bool WriteFloat(std::ofstream& out, float value) { return WriteBytes(out, &value, sizeof(value)); }
    bool ReadFloat(std::ifstream& in, float& value) { return ReadBytes(in, &value, sizeof(value)); }

    bool WriteInt(std::ofstream& out, int value) { return WriteBytes(out, &value, sizeof(value)); }
    bool ReadInt(std::ifstream& in, int& value) { return ReadBytes(in, &value, sizeof(value)); }
}

std::vector<std::string> SaveSystem::listSaves() const {
    std::vector<std::string> result;

    std::error_code ec;
    if (!fs::exists(baseFolder, ec)) {
        fs::create_directories(baseFolder, ec);
    }

    for (const auto& entry : fs::directory_iterator(baseFolder, ec)) {
        if (entry.is_directory()) {
            result.push_back(entry.path().filename().string());
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

bool SaveSystem::deleteSave(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    std::error_code ec;
    const auto target = baseFolder / name;
    if (!fs::exists(target, ec)) {
        return false;
    }

    fs::remove_all(target, ec);
    return !ec;
}

bool SaveSystem::openOrCreate(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    currentSaveName = name;
    paths.saveRoot = baseFolder / name;
    paths.regionsRoot = paths.saveRoot / "regions";
    paths.dataFile = paths.saveRoot / "save.dat";

    std::error_code ec;
    fs::create_directories(paths.regionsRoot, ec);
    return !ec;
}

bool SaveSystem::createNew(const std::string& name) {
    if (!openOrCreate(name)) {
        return false;
    }

    std::error_code ec;
    if (fs::exists(paths.regionsRoot, ec)) {
        for (const auto& entry : fs::directory_iterator(paths.regionsRoot, ec)) {
            fs::remove_all(entry.path(), ec);
        }
    }
    if (fs::exists(paths.dataFile, ec)) {
        fs::remove(paths.dataFile, ec);
    }

    return !ec;
}

bool SaveSystem::saveGame(const Settings& settings, const GameManager& gameManager, const yc::world::World& world) {
    if (paths.saveRoot.empty()) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(paths.regionsRoot, ec);
    if (ec) {
        return false;
    }

    std::ofstream out(paths.dataFile, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    SaveHeader header{};
    if (!WriteBytes(out, &header, sizeof(header))) return false;

    const auto& ws = world.getSettings();
    if (!WriteInt(out, ws.viewDistance)) return false;
    if (!WriteInt(out, ws.maxUnloadChunkPerFrame)) return false;
    if (!WriteInt(out, ws.maxChunksLoadPerFrame)) return false;

    if (!WriteDouble(out, gameManager.getSimTimeSec())) return false;
    if (!WriteDouble(out, gameManager.getTimeScale())) return false;

    const uint8_t running = gameManager.isSimulationRunning() ? 1 : 0;
    if (!WriteU8(out, running)) return false;

    if (!WriteFloat(out, gameManager.getTemperatureC())) return false;
    if (!WriteFloat(out, world.getExposureScale())) return false;

    const auto& chimneys = world.getChimneyEmitters();
    if (!WriteU32(out, static_cast<uint32_t>(chimneys.size()))) return false;

    for (const auto& c : chimneys) {
        if (!WriteDouble(out, c.worldPos.x)) return false;
        if (!WriteDouble(out, c.worldPos.y)) return false;
        if (!WriteDouble(out, c.worldPos.z)) return false;
        if (!WriteDouble(out, c.height)) return false;
        if (!WriteDouble(out, c.exitVelocity)) return false;
        if (!WriteDouble(out, c.radius)) return false;
    }

    out.flush();
    return out.good();
}

bool SaveSystem::loadGame(Settings& settings, GameManager& gameManager, yc::world::World& world) {
    if (paths.dataFile.empty() || !fs::exists(paths.dataFile)) {
        return false;
    }

    std::ifstream in(paths.dataFile, std::ios::binary | std::ios::in);
    if (!in.is_open()) {
        return false;
    }

    SaveHeader header{};
    if (!ReadBytes(in, &header, sizeof(header))) return false;
    if (header.magic != SaveMagic || header.version != SaveVersion) return false;

    Settings::WorldSettings ws{};
    if (!ReadInt(in, ws.viewDistance)) return false;
    if (!ReadInt(in, ws.maxUnloadChunkPerFrame)) return false;
    if (!ReadInt(in, ws.maxChunksLoadPerFrame)) return false;

    double simTimeSec = 0.0;
    double timeScale = 1.0;
    if (!ReadDouble(in, simTimeSec)) return false;
    if (!ReadDouble(in, timeScale)) return false;

    uint8_t running = 0;
    if (!ReadU8(in, running)) return false;

    float temperatureC = 20.0f;
    float exposureScale = 0.01f;
    if (!ReadFloat(in, temperatureC)) return false;
    if (!ReadFloat(in, exposureScale)) return false;

    uint32_t chimneyCount = 0;
    if (!ReadU32(in, chimneyCount)) return false;

    std::vector<yc::world::ChimneySource> chimneys;
    chimneys.reserve(chimneyCount);

    for (uint32_t i = 0; i < chimneyCount; ++i) {
        yc::world::ChimneySource c{};
        if (!ReadDouble(in, c.worldPos.x)) return false;
        if (!ReadDouble(in, c.worldPos.y)) return false;
        if (!ReadDouble(in, c.worldPos.z)) return false;
        if (!ReadDouble(in, c.height)) return false;
        if (!ReadDouble(in, c.exitVelocity)) return false;
        if (!ReadDouble(in, c.radius)) return false;
        chimneys.push_back(c);
    }

    settings.world = ws;
    settings.game.timeScale = timeScale;
    settings.game.temperatureC = temperatureC;
    settings.exposure.exposureScale = exposureScale;

    world.setSettings(ws);
    world.setExposureScale(exposureScale);
    world.setChimneyEmitters(chimneys);

    gameManager.setTimeScale(timeScale);
    gameManager.setTemperatureC(temperatureC);
    gameManager.setExposureScale(exposureScale);
    gameManager.setSimTimeSec(simTimeSec);
    gameManager.setSimulationRunning(running != 0);

    return true;
}

bool SaveSystem::hasSaveData() const {
    if (paths.dataFile.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::exists(paths.dataFile, ec);
}

}