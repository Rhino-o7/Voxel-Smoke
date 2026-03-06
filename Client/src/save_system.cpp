#include "save_system.h"

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace yc {

namespace fs = std::filesystem;

namespace {
    constexpr uint32_t SaveMagic = 0x56534359; // "YCSV"
    constexpr uint32_t SaveVersion = 9;

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
    if (!WriteInt(out, world.getSeed())) return false;

    if (!WriteDouble(out, gameManager.getSimTimeSec())) return false;
    if (!WriteDouble(out, gameManager.getTimeScale())) return false;

    const uint8_t running = gameManager.isSimulationRunning() ? 1 : 0;
    if (!WriteU8(out, running)) return false;

    if (!WriteFloat(out, gameManager.getTemperatureC())) return false;
    if (!WriteFloat(out, world.getExposureScale())) return false;
    if (!WriteFloat(out, settings.ui.scale)) return false;
    if (!WriteFloat(out, settings.player.moveSpeed)) return false;
    if (!WriteFloat(out, settings.player.gravityMultiplier)) return false;
    if (!WriteFloat(out, settings.player.jumpHeight)) return false;
    if (!WriteFloat(out, settings.lighting.sunDirectionX)) return false;
    if (!WriteFloat(out, settings.lighting.sunDirectionY)) return false;
    if (!WriteFloat(out, settings.lighting.sunDirectionZ)) return false;
    if (!WriteFloat(out, settings.lighting.sunriseStartHour)) return false;
    if (!WriteFloat(out, settings.lighting.dayStartHour)) return false;
    if (!WriteFloat(out, settings.lighting.dayEndHour)) return false;
    if (!WriteFloat(out, settings.lighting.nightStartHour)) return false;
    if (!WriteFloat(out, settings.lighting.ambientNight)) return false;
    if (!WriteFloat(out, settings.lighting.ambientDay)) return false;
    if (!WriteFloat(out, settings.lighting.diffuseNight)) return false;
    if (!WriteFloat(out, settings.lighting.diffuseDay)) return false;
    if (!WriteFloat(out, settings.lighting.specularNight)) return false;
    if (!WriteFloat(out, settings.lighting.specularDay)) return false;
    if (!WriteFloat(out, settings.lighting.shininess)) return false;
    if (!WriteFloat(out, settings.lighting.waterTintR)) return false;
    if (!WriteFloat(out, settings.lighting.waterTintG)) return false;
    if (!WriteFloat(out, settings.lighting.waterTintB)) return false;
    if (!WriteFloat(out, settings.lighting.waterDiffuseMul)) return false;
    if (!WriteFloat(out, settings.lighting.waterSpecularMul)) return false;
    if (!WriteFloat(out, settings.lighting.waterMinAlpha)) return false;
    if (!WriteU8(out, gameManager.isDayNightCycleEnabled() ? 1 : 0)) return false;
    if (!WriteInt(out, gameManager.getFarmingYear())) return false;

    const auto date = gameManager.getDate();
    const auto tod = gameManager.getTimeOfDay();
    if (!WriteInt(out, static_cast<int>(date.season))) return false;
    if (!WriteInt(out, tod.hours)) return false;

    const auto& chimneys = world.getChimneyEmitters();
    if (!WriteU32(out, static_cast<uint32_t>(chimneys.size()))) return false;

    for (const auto& c : chimneys) {
        if (!WriteDouble(out, c.worldPos.x)) return false;
        if (!WriteDouble(out, c.worldPos.y)) return false;
        if (!WriteDouble(out, c.worldPos.z)) return false;
        if (!WriteInt(out, c.baseBlockCoord.x)) return false;
        if (!WriteInt(out, c.baseBlockCoord.y)) return false;
        if (!WriteInt(out, c.baseBlockCoord.z)) return false;
        if (!WriteDouble(out, c.height)) return false;
        if (!WriteDouble(out, c.exitVelocity)) return false;
        if (!WriteDouble(out, c.radius)) return false;
        if (!WriteU8(out, c.enabled ? 1 : 0)) return false;
    }

    const auto& cropExposure = gameManager.getCropExposureByBlock();
    if (!WriteU32(out, static_cast<uint32_t>(cropExposure.size()))) return false;

    for (const auto& [pos, exposure] : cropExposure) {
        if (!WriteInt(out, pos.x)) return false;
        if (!WriteInt(out, pos.y)) return false;
        if (!WriteInt(out, pos.z)) return false;
        if (!WriteDouble(out, exposure)) return false;
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
    if (header.magic != SaveMagic || header.version == 0 || header.version > SaveVersion) return false;

    Settings::WorldSettings ws{};
    if (!ReadInt(in, ws.viewDistance)) return false;
    if (!ReadInt(in, ws.maxUnloadChunkPerFrame)) return false;
    if (!ReadInt(in, ws.maxChunksLoadPerFrame)) return false;

    int worldSeed = 0;
    if (header.version >= 6) {
        if (!ReadInt(in, worldSeed)) return false;
    }

    double simTimeSec = 0.0;
    double timeScale = 1.0;
    if (!ReadDouble(in, simTimeSec)) return false;
    if (!ReadDouble(in, timeScale)) return false;

    uint8_t running = 0;
    if (!ReadU8(in, running)) return false;

    float temperatureC = 20.0f;
    float exposureScale = 0.01f;
    float uiScale = 1.0f;
    float moveSpeed = settings.player.moveSpeed;
    float gravityMultiplier = settings.player.gravityMultiplier;
    float jumpHeight = settings.player.jumpHeight;
    if (!ReadFloat(in, temperatureC)) return false;
    if (!ReadFloat(in, exposureScale)) return false;
    if (header.version >= 7) {
        if (!ReadFloat(in, uiScale)) return false;
    }
    if (header.version >= 8) {
        if (!ReadFloat(in, moveSpeed)) return false;
        if (!ReadFloat(in, gravityMultiplier)) return false;
        if (!ReadFloat(in, jumpHeight)) return false;
    }
    if (header.version >= 9) {
        if (!ReadFloat(in, settings.lighting.sunDirectionX)) return false;
        if (!ReadFloat(in, settings.lighting.sunDirectionY)) return false;
        if (!ReadFloat(in, settings.lighting.sunDirectionZ)) return false;
        if (!ReadFloat(in, settings.lighting.sunriseStartHour)) return false;
        if (!ReadFloat(in, settings.lighting.dayStartHour)) return false;
        if (!ReadFloat(in, settings.lighting.dayEndHour)) return false;
        if (!ReadFloat(in, settings.lighting.nightStartHour)) return false;
        if (!ReadFloat(in, settings.lighting.ambientNight)) return false;
        if (!ReadFloat(in, settings.lighting.ambientDay)) return false;
        if (!ReadFloat(in, settings.lighting.diffuseNight)) return false;
        if (!ReadFloat(in, settings.lighting.diffuseDay)) return false;
        if (!ReadFloat(in, settings.lighting.specularNight)) return false;
        if (!ReadFloat(in, settings.lighting.specularDay)) return false;
        if (!ReadFloat(in, settings.lighting.shininess)) return false;
        if (!ReadFloat(in, settings.lighting.waterTintR)) return false;
        if (!ReadFloat(in, settings.lighting.waterTintG)) return false;
        if (!ReadFloat(in, settings.lighting.waterTintB)) return false;
        if (!ReadFloat(in, settings.lighting.waterDiffuseMul)) return false;
        if (!ReadFloat(in, settings.lighting.waterSpecularMul)) return false;
        if (!ReadFloat(in, settings.lighting.waterMinAlpha)) return false;
    }

    bool dayNightCycleEnabled = true;
    int farmingYear = 1;
    int currentSeason = 0;
    int startHour = 8;
    if (header.version >= 4) {
        uint8_t dayNightCycle = 1;
        if (!ReadU8(in, dayNightCycle)) return false;
        dayNightCycleEnabled = (dayNightCycle != 0);
        if (header.version >= 5) {
            if (!ReadInt(in, farmingYear)) return false;
        }
        if (!ReadInt(in, currentSeason)) return false;
        if (!ReadInt(in, startHour)) return false;
    }

    uint32_t chimneyCount = 0;
    if (!ReadU32(in, chimneyCount)) return false;

    std::vector<yc::world::ChimneySource> chimneys;
    chimneys.reserve(chimneyCount);

    for (uint32_t i = 0; i < chimneyCount; ++i) {
        yc::world::ChimneySource c{};
        if (!ReadDouble(in, c.worldPos.x)) return false;
        if (!ReadDouble(in, c.worldPos.y)) return false;
        if (!ReadDouble(in, c.worldPos.z)) return false;
        if (header.version >= 3) {
            if (!ReadInt(in, c.baseBlockCoord.x)) return false;
            if (!ReadInt(in, c.baseBlockCoord.y)) return false;
            if (!ReadInt(in, c.baseBlockCoord.z)) return false;
        } else {
            c.baseBlockCoord = {
                static_cast<int>(std::floor(c.worldPos.x)),
                static_cast<int>(std::floor(c.worldPos.y)),
                static_cast<int>(std::floor(c.worldPos.z))
            };
        }
        if (!ReadDouble(in, c.height)) return false;
        if (!ReadDouble(in, c.exitVelocity)) return false;
        if (!ReadDouble(in, c.radius)) return false;
        if (header.version >= 3) {
            uint8_t enabled = 1;
            if (!ReadU8(in, enabled)) return false;
            c.enabled = (enabled != 0);
        } else {
            c.enabled = true;
        }
        chimneys.push_back(c);
    }

    yc::world::CropExposureMap cropExposure;
    if (header.version >= 2) {
        uint32_t cropCount = 0;
        if (!ReadU32(in, cropCount)) return false;

        for (uint32_t i = 0; i < cropCount; ++i) {
            int x = 0, y = 0, z = 0;
            double exposure = 0.0;
            if (!ReadInt(in, x)) return false;
            if (!ReadInt(in, y)) return false;
            if (!ReadInt(in, z)) return false;
            if (!ReadDouble(in, exposure)) return false;
            cropExposure[{ x, y, z }] = exposure;
        }
    }

    settings.world = ws;
    settings.game.timeScale = timeScale;
    settings.game.temperatureC = temperatureC;
    settings.exposure.exposureScale = exposureScale;
    settings.ui.scale = std::clamp(uiScale, 0.75f, 2.0f);
    settings.player.moveSpeed = std::clamp(moveSpeed, 1.0f, 200.0f);
    settings.player.gravityMultiplier = std::clamp(gravityMultiplier, 0.1f, 10.0f);
    settings.player.jumpHeight = std::clamp(jumpHeight, 0.5f, 4.0f);
    settings.game.dayNightCycleEnabled = dayNightCycleEnabled;
    settings.game.currentSeason = std::clamp(currentSeason, 0, 3);
    settings.game.startHour = std::clamp(startHour, 0, 23);

    world.setSettings(ws);
    world.setLightingSettings(settings.lighting);
    world.setExposureScale(exposureScale);
    world.setSeed(worldSeed);
    world.setChimneyEmitters(chimneys);

    gameManager.setTimeScale(timeScale);
    gameManager.setTemperatureC(temperatureC);
    gameManager.setExposureScale(exposureScale);
    gameManager.setDayNightCycleEnabled(dayNightCycleEnabled);
    gameManager.setFarmingYear(farmingYear);
    gameManager.setSimTimeSec(simTimeSec);
    gameManager.setSimulationRunning(running != 0);
    gameManager.setCropExposureByBlock(cropExposure);

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