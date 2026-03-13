#include "game_manager.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <unordered_set>

namespace yc {

int GameManager::getSeasonStartDay(Season season) {
    return static_cast<int>(season) * DaysPerSeason;
}

int GameManager::positiveMod(int value, int mod) {
    const int r = value % mod;
    return (r < 0) ? (r + mod) : r;
}

GameManager::GameManager() {
    clock.setTimeScale(10.0); // 1s real = 10s game
    setCurrentSeason(Season::Spring, false);
    setCurrentHour(8);
}

void GameManager::init(yc::world::World* world, Player* player) {
    this->world = world;
    this->player = player;

    if (world) {
        world->setWindState(currentWindState);
        world->setCropExposureMap(&cropExposureByBlock);
        world->setSimTimeSec(clock.getSimTimeSec());
    }
}

void GameManager::update(double realDtSec) {
    // Convert real frame delta to simulation delta when simulation is active.
    double simDtSec = 0.0;

    if (simulationRunning) {
        simDtSec = clock.tick(realDtSec);
        wind.update(clock.getSimTimeSec());
        currentWindState = wind.current();
    }

    pollution.setWind(currentWindState);

    if (world) {
        world->setWindState(currentWindState);
        world->setSimTimeSec(clock.getSimTimeSec());
    }

    updateCalendarState();
    finishHarvestIfNeeded();

    if (pendingCropRegistrationTicks > 0) {
        // Deferred registration helps after loading chunks so crops become tracked incrementally.
        if ((pendingCropRegistrationTicks % 10) == 0) {
            registerLoadedCropBlocks();
        }
        --pendingCropRegistrationTicks;
    }

    if (realDtSec > 0.0) {
        cropChunkRebuildAccumulatorSec += realDtSec;
        if (cropChunkRebuildAccumulatorSec >= CropChunkRebuildPeriodSec) {
            cropChunkRebuildAccumulatorSec = std::fmod(cropChunkRebuildAccumulatorSec, CropChunkRebuildPeriodSec);
            rebuildOneCropChunk();
        }
    }

    if (simulationRunning && simDtSec > 0.0) {
        // Advance crop exposure in fixed-size simulation steps for stable behavior.
        cropExposureAccumulatorSec += simDtSec;

        constexpr int MaxExposureStepsPerFrame = 8;
        int steps = 0;
        while (cropExposureAccumulatorSec >= CropExposureUpdateStepSec && steps < MaxExposureStepsPerFrame) {
            updateCropExposure(CropExposureUpdateStepSec);
            cropExposureAccumulatorSec -= CropExposureUpdateStepSec;
            ++steps;
        }

        if (steps == MaxExposureStepsPerFrame && cropExposureAccumulatorSec >= CropExposureUpdateStepSec) {
            cropExposureAccumulatorSec = std::fmod(cropExposureAccumulatorSec, CropExposureUpdateStepSec);
        }
    }
}

void GameManager::pauseTime() {
    resumeOnUnpause = simulationRunning;
    stopSimulation();
}

void GameManager::skipToEndOfCurrentSeason() {
    if (gameOver) {
        return;
    }

    const int secondsPerDay = HoursPerDay * 3600;
    const double currentSimTime = getSimTimeSec();
    const int absoluteDay = static_cast<int>(std::floor(currentSimTime / static_cast<double>(secondsPerDay)));
    const int yearIndex = std::max(0, absoluteDay / DaysPerYear);
    int targetAbsoluteDay = yearIndex * DaysPerYear + getSeasonStartDay(Season::Autumn);
    if (absoluteDay >= targetAbsoluteDay) {
        targetAbsoluteDay += DaysPerYear;
    }

    const double targetSimTime = static_cast<double>(targetAbsoluteDay * secondsPerDay);

    if (currentSimTime >= targetSimTime) {
        return;
    }

    registerLoadedCropBlocks();

    // Step through simulation in fixed chunks so wind/pollution systems evolve consistently.
    constexpr double integrationStepSec = 900.0;
    double simTime = currentSimTime;

    while (simTime < targetSimTime) {
        const double step = std::min(integrationStepSec, targetSimTime - simTime);
        simTime += step;
        clock.setSimTimeSec(simTime);

        wind.update(clock.getSimTimeSec());
        currentWindState = wind.current();
        pollution.setWind(currentWindState);

        if (world) {
            world->setWindState(currentWindState);
            world->setSimTimeSec(clock.getSimTimeSec());
        }

        updateCalendarState();
        finishHarvestIfNeeded();
        updateCropExposure(step, false);

        if (gameOver) {
            break;
        }
    }

    refreshCropExposureBuffers();
    finishHarvestIfNeeded();
}

void GameManager::resumeTime() {
    if (resumeOnUnpause) {
        startSimulation();
    }
    resumeOnUnpause = false;
}

void GameManager::startSimulation() {
    if (simulationRunning) return;
    simulationRunning = true;
    wind.update(clock.getSimTimeSec());
    currentWindState = wind.current();
    if (world) {
        world->setWindState(currentWindState);
    }
}

void GameManager::scheduleLoadedCropRegistration(int ticks) {
    pendingCropRegistrationTicks = std::max(0, ticks);
}

void GameManager::stopSimulation() {
    simulationRunning = false;
}

void GameManager::toggleSimulation() {
    if (simulationRunning) {
        stopSimulation();
    } else {
        startSimulation();
    }
}

bool GameManager::loadWindCsv(const std::string& path) {
    namespace fs = std::filesystem;

    std::vector<std::string> candidates;
    candidates.push_back(path);

    const fs::path fileName = fs::path(path).filename();

    // Prefer runtime resources folder first.
    candidates.push_back((fs::path("./resources/simulation") / fileName).string());
    candidates.push_back((fs::path("../resources/simulation") / fileName).string());
    candidates.push_back((fs::path("../../resources/simulation") / fileName).string());

    // Compatibility fallbacks (older layouts).
    candidates.push_back((fs::path("./CoreLib/resources/simulation") / fileName).string());
    candidates.push_back((fs::path("../CoreLib/resources/simulation") / fileName).string());
    candidates.push_back((fs::path("../../CoreLib/resources/simulation") / fileName).string());

    bool loaded = false;
    for (const auto& candidate : candidates) {
        if (wind.loadFromCsvFile(candidate)) {
            loaded = true;
            break;
        }
    }

    if (loaded && simulationRunning) {
        wind.update(clock.getSimTimeSec());
        currentWindState = wind.current();
        if (world) {
            world->setWindState(currentWindState);
        }
    }
    return loaded;
}

GameManager::TimeOfDay GameManager::getTimeOfDay() const
{
    const int totalSeconds = static_cast<int>(std::floor(getSimTimeSec()));
    const int secondsInDay = HoursPerDay * 3600;
    const int daySeconds = positiveMod(totalSeconds, secondsInDay);

    return {
        daySeconds / 3600,
        (daySeconds / 60) % 60,
        daySeconds % 60
    };
}

GameManager::Date GameManager::getDate() const {
    const int absoluteDay = static_cast<int>(std::floor(getSimTimeSec() / (HoursPerDay * 3600.0)));
    const int dayInYearZeroBased = positiveMod(absoluteDay, DaysPerYear);

    const Season season = static_cast<Season>(dayInYearZeroBased / DaysPerSeason);
    const int dayOfSeason = (dayInYearZeroBased % DaysPerSeason) + 1;
    const int hour = getTimeOfDay().hours;

    Date date{};
    date.year = farmingYear;
    date.dayOfYear = dayInYearZeroBased + 1;
    date.dayOfSeason = dayOfSeason;
    date.season = season;
    date.isDaytime = !dayNightCycleEnabled || (hour >= 6 && hour < 18);
    return date;
}

GameManager::Season GameManager::getCurrentSeason() const {
    return getDate().season;
}

const char* GameManager::getSeasonName(Season season) {
    switch (season) {
    case Season::Spring: return "Spring";
    case Season::Summer: return "Summer";
    case Season::Autumn: return "Autumn";
    case Season::Winter: return "Winter";
    default: return "Unknown";
    }
}

void GameManager::setCurrentSeason(Season season, bool keepTimeOfDay) {
    const int hour = keepTimeOfDay ? getTimeOfDay().hours : 0;
    const int minute = keepTimeOfDay ? getTimeOfDay().minutes : 0;
    const int second = keepTimeOfDay ? getTimeOfDay().seconds : 0;

    const int absoluteDay = static_cast<int>(std::floor(getSimTimeSec() / (HoursPerDay * 3600.0)));
    const int yearIndex = std::max(0, absoluteDay / DaysPerYear);
    const int newDay = yearIndex * DaysPerYear + getSeasonStartDay(season);
    const int daySeconds = hour * 3600 + minute * 60 + second;

    clock.setSimTimeSec(static_cast<double>(newDay * HoursPerDay * 3600 + daySeconds));
    updateCalendarState();
}

void GameManager::setCurrentHour(int hour) {
    const int clampedHour = std::clamp(hour, 0, 23);
    const auto tod = getTimeOfDay();
    const int absoluteDay = static_cast<int>(std::floor(getSimTimeSec() / (HoursPerDay * 3600.0)));
    const int daySeconds = clampedHour * 3600 + tod.minutes * 60 + tod.seconds;
    clock.setSimTimeSec(static_cast<double>(absoluteDay * HoursPerDay * 3600 + daySeconds));
    updateCalendarState();
}

double GameManager::getPollutionAtWorld(const yc::world::WorldPos& worldPos) const {
    if (!world) return 0.0;

    double total = 0.0;
    yc::world::PollutionSystem ps;
    ps.setWind(currentWindState);

    const auto& sources = world->getChimneyEmitters();
    for (const auto& src : sources) {
        if (!src.enabled) {
            continue;
        }
        ps.setSource(src);
        total += ps.concentrationAt(worldPos);
    }

    return total;
}

double GameManager::getPollutionAtBlock(const yc::world::BlockPos& blockPos) const {
    if (!world) return 0.0;
    return getPollutionAtWorld(yc::world::World::getBlockToWorldCoord(blockPos));
}

void GameManager::registerCropBlock(const yc::world::BlockPos& blockPos) {
    cropExposureByBlock.try_emplace(blockPos, 0.0);
    cropChunkRebuildList.clear();
    nextCropChunkRebuildIndex = 0;

    if (!world) {
        return;
    }

    const auto chunkCoord = yc::world::World::GetChunkCoordOf(blockPos);
    if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
        chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
            return static_cast<float>(getCropExposureAtBlock(worldCoord));
        });
    }
}

void GameManager::unregisterCropBlock(const yc::world::BlockPos& blockPos) {
    const size_t erased = cropExposureByBlock.erase(blockPos);
    cropChunkRebuildList.clear();
    nextCropChunkRebuildIndex = 0;
    if (erased == 0 || !world) {
        return;
    }

    const auto chunkCoord = yc::world::World::GetChunkCoordOf(blockPos);
    if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
        chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
            return static_cast<float>(getCropExposureAtBlock(worldCoord));
        });
    }
}

void GameManager::registerLoadedCropBlocks() {
    if (!world) {
        return;
    }

    // Pull currently loaded crop blocks into the exposure map and refresh affected chunk buffers.
    std::unordered_set<glm::ivec2, yc::world::World::HashChunkCoord> dirtyChunks;
    const auto loadedCropBlocks = world->getLoadedBlockPositionsOfType(yc::world::BlockType::CROP);
    for (const auto& blockPos : loadedCropBlocks) {
        const auto [it, inserted] = cropExposureByBlock.try_emplace(blockPos, 0.0);
        (void)it;
        if (inserted) {
            dirtyChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
        }
    }

    for (const auto& chunkCoord : dirtyChunks) {
        if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
            chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
                return static_cast<float>(getCropExposureAtBlock(worldCoord));
            });
        }
    }

    if (!dirtyChunks.empty()) {
        cropChunkRebuildList.clear();
        nextCropChunkRebuildIndex = 0;
    }
}

void GameManager::setCropExposureByBlock(const yc::world::CropExposureMap& map) {
    cropExposureByBlock = map;
    cropChunkRebuildList.clear();
    nextCropChunkRebuildIndex = 0;
    refreshCropExposureBuffers();
}

double GameManager::getCropExposureAtBlock(const yc::world::BlockPos& blockPos) const {
    auto it = cropExposureByBlock.find(blockPos);
    return (it != cropExposureByBlock.end()) ? it->second : 0.0;
}

float GameManager::getCropHealthPercent() const {
    // Convert average exposure to a [0..1] health indicator used by UI/gameplay.
    if (cropExposureByBlock.empty()) return 1.0f;

    const double scale = std::max(1e-9, static_cast<double>(exposureScale));
    double sum = 0.0;

    for (const auto& [blockPos, exposure] : cropExposureByBlock) {
        const double darken = std::clamp(exposure * scale, 0.0, 1.0);
        sum += darken;
    }

    const double avgDarken = sum / static_cast<double>(cropExposureByBlock.size());
    return static_cast<float>(std::clamp(1.0 - avgDarken, 0.0, 1.0));
}

float GameManager::getCropScoreAtBlock(const yc::world::BlockPos& blockPos) const {
    const double exposure = getCropExposureAtBlock(blockPos);
    const double scale = std::max(1e-9, static_cast<double>(exposureScale));
    const double darken = std::clamp(exposure * scale, 0.0, 1.0);
    return static_cast<float>((1.0 - darken) * 100.0);
}

void GameManager::resetRound() {
    farmingYear = 1;
    gameOver = false;
    harvestResult = {};
    lastAbsoluteDay = -1;
    cropExposureByBlock.clear();
    cropExposureAccumulatorSec = 0.0;
    cropChunkRebuildAccumulatorSec = 0.0;
    cropChunkRebuildList.clear();
    nextCropChunkRebuildIndex = 0;
    setCurrentSeason(Season::Spring, false);
    setCurrentHour(8);
    stopSimulation();
}

void GameManager::continueToNextFarmingYear() {
    const int secondsPerDay = HoursPerDay * 3600;
    const int nextStartDay = getSeasonStartDay(Season::Spring);
    const int startHour = 8;
    const int daySeconds = startHour * 3600;

    std::unordered_set<glm::ivec2, yc::world::World::HashChunkCoord> dirtyCropChunks;
    for (const auto& [blockPos, exposure] : cropExposureByBlock) {
        (void)exposure;
        dirtyCropChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
    }

    farmingYear += 1;
    cropExposureByBlock.clear();
    cropExposureAccumulatorSec = 0.0;
    cropChunkRebuildAccumulatorSec = 0.0;
    cropChunkRebuildList.clear();
    nextCropChunkRebuildIndex = 0;
    gameOver = false;
    harvestResult = {};
    lastAbsoluteDay = -1;

    clock.setSimTimeSec(static_cast<double>(nextStartDay * secondsPerDay + daySeconds));

    if (world) {
        const auto loadedCropBlocks = world->getLoadedBlockPositionsOfType(yc::world::BlockType::CROP);
        for (const auto& blockPos : loadedCropBlocks) {
            cropExposureByBlock[blockPos] = 0.0;
            dirtyCropChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
        }

        world->setSimTimeSec(clock.getSimTimeSec());

        for (const auto& chunkCoord : dirtyCropChunks) {
            if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
                chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
                    return static_cast<float>(getCropExposureAtBlock(worldCoord));
                });
            }
        }
    }

    updateCalendarState();
    stopSimulation();
}

void GameManager::updateCalendarState() {
    const int absoluteDay = static_cast<int>(std::floor(getSimTimeSec() / (HoursPerDay * 3600.0)));
    if (absoluteDay != lastAbsoluteDay) {
        lastAbsoluteDay = absoluteDay;
    }
}

void GameManager::finishHarvestIfNeeded() {
    if (gameOver) {
        return;
    }

    const auto date = getDate();
    // Harvest resolves once per farming year at the beginning of Autumn.
    if (date.season != Season::Autumn || date.dayOfSeason != 1) {
        return;
    }

    HarvestResult result{};
    result.hasResult = true;
    result.cropCount = static_cast<int>(cropExposureByBlock.size());
    constexpr int MaxCropsPerBlock = 10;
    result.maxCrops = result.cropCount * MaxCropsPerBlock;

    int produced = 0;
    for (const auto& [blockPos, exposure] : cropExposureByBlock) {
        (void)blockPos;
        const double scale = std::max(1e-9, static_cast<double>(exposureScale));
        const double darken = std::clamp(exposure * scale, 0.0, 1.0);
        produced += static_cast<int>(std::round((1.0 - darken) * static_cast<double>(MaxCropsPerBlock)));
    }

    result.producedCrops = std::clamp(produced, 0, result.maxCrops);

    harvestResult = result;
    gameOver = true;
    resumeOnUnpause = false;
    stopSimulation();
}

void GameManager::refreshCropExposureBuffers() {
    if (!world || cropExposureByBlock.empty()) {
        return;
    }

    std::unordered_set<glm::ivec2, yc::world::World::HashChunkCoord> dirtyChunks;
    for (const auto& [blockPos, exposure] : cropExposureByBlock) {
        (void)exposure;
        dirtyChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
    }

    for (const auto& chunkCoord : dirtyChunks) {
        if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
            chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
                return static_cast<float>(getCropExposureAtBlock(worldCoord));
            });
        }
    }
}

void GameManager::rebuildOneCropChunk() {
    if (!world) {
        return;
    }

    if (cropExposureByBlock.empty()) {
        cropChunkRebuildList.clear();
        nextCropChunkRebuildIndex = 0;
        return;
    }

    if (cropChunkRebuildList.empty() || nextCropChunkRebuildIndex >= cropChunkRebuildList.size()) {
        std::unordered_set<glm::ivec2, yc::world::World::HashChunkCoord> chunkSet;
        for (const auto& [blockPos, exposure] : cropExposureByBlock) {
            (void)exposure;
            chunkSet.insert(yc::world::World::GetChunkCoordOf(blockPos));
        }

        cropChunkRebuildList.assign(chunkSet.begin(), chunkSet.end());
        nextCropChunkRebuildIndex = 0;
    }

    if (cropChunkRebuildList.empty()) {
        return;
    }

    const auto chunkCoord = cropChunkRebuildList[nextCropChunkRebuildIndex++];
    if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
        chunk->prepareToBuildMesh();
    }
}

void GameManager::updateCropExposure(double simDtSec, bool updateChunkBuffers) {
    if (!world) return;

    if (simDtSec <= 0.0 || cropExposureByBlock.empty()) return;

    const auto& sources = world->getChimneyEmitters();
    if (sources.empty()) return;

    yc::world::PollutionSystem ps;
    ps.setWind(currentWindState);

    std::vector<yc::world::BlockPos> toRemove;
    std::unordered_set<glm::ivec2, yc::world::World::HashChunkCoord> dirtyChunks;

    for (auto& [blockPos, exposure] : cropExposureByBlock) {
        const auto blockData = world->getBlockDataIfLoadedAt(blockPos);

        if (blockData.getType() == yc::world::BlockType::NONE) {
            continue;
        }

        if (blockData.getType() != yc::world::BlockType::CROP) {
            toRemove.push_back(blockPos);
            continue;
        }

        const auto worldPos = yc::world::World::getBlockToWorldCoord(blockPos);

        double total = 0.0;
        for (const auto& src : sources) {
            if (!src.enabled) {
                continue;
            }
            ps.setSource(src);
            total += ps.concentrationAt(worldPos);
        }

        exposure += total * simDtSec;
        dirtyChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
    }

    for (const auto& pos : toRemove) {
        cropExposureByBlock.erase(pos);
    }

    if (!updateChunkBuffers) {
        return;
    }

    for (const auto& chunkCoord : dirtyChunks) {
        if (auto chunk = world->getChunkIfLoadedAt(chunkCoord)) {
            chunk->updateCropExposureBuffers([this](const glm::ivec3& worldCoord) {
                return static_cast<float>(getCropExposureAtBlock(worldCoord));
            });
        }
    }
}

void GameManager::applySettings(const Settings& settings) {
    clock.setTimeScale(settings.game.timeScale);
    temperatureC = settings.game.temperatureC;
    exposureScale = settings.exposure.exposureScale;
    dayNightCycleEnabled = settings.game.dayNightCycleEnabled;

    if (world) {
        world->setSettings(settings.world);
        world->setLightingSettings(settings.lighting);
        world->setExposureScale(settings.exposure.exposureScale);
        world->setSmokeSettings(settings.smoke);
    }

    chimneySettings.height = settings.chimney.height;
    chimneySettings.radius = settings.chimney.radius;
    chimneySettings.exitVelocity = settings.chimney.exitVelocity;

    if (!settings.game.windCsvPath.empty()) {
        loadWindCsv(settings.game.windCsvPath);
    }

    if (settings.game.startSimulationRunning && !simulationRunning) {
        startSimulation();
    } else if (!settings.game.startSimulationRunning && simulationRunning) {
        stopSimulation();
    }
}

void GameManager::setSimulationRunning(bool value) {
    if (value) {
        startSimulation();
    } else {
        stopSimulation();
    }
}

}