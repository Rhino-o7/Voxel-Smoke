#include "game_manager.h"

#include <algorithm>
#include <vector>
#include <unordered_set>

namespace yc {

GameManager::GameManager() {
    clock.setTimeScale(10.0); // 1s real = 10s game
}

void GameManager::init(yc::world::World* world, Player* player) {
    this->world = world;
    this->player = player;

    if (world) {
        world->setWindState(currentWindState);
        world->setCropExposureMap(&cropExposureByBlock);
    }
}

void GameManager::update(double realDtSec) {
    double simDtSec = 0.0;

    if (simulationRunning) {
        simDtSec = clock.tick(realDtSec);
        wind.update(clock.getSimTimeSec());
        currentWindState = wind.current();
    }

    pollution.setWind(currentWindState);

    if (world) {
        world->setWindState(currentWindState);
    }

    updateCropExposure(simDtSec);
}

void GameManager::pauseTime() {
    resumeOnUnpause = simulationRunning;
    stopSimulation();
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
    const bool loaded = wind.loadFromCsvFile(path);
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
    return {
        static_cast<int>(std::fmod(std::floor(getSimTimeSec() / 3600.0), 24.0)),
        static_cast<int>(std::fmod(std::floor(getSimTimeSec() / 60.0), 60.0)),
        static_cast<int>(std::fmod(std::floor(getSimTimeSec()), 60.0))
	};
}

double GameManager::getPollutionAtWorld(const yc::world::WorldPos& worldPos) const {
    if (!world) return 0.0;

    double total = 0.0;
    yc::world::PollutionSystem ps;
    ps.setWind(currentWindState);

    const auto& sources = world->getChimneyEmitters();
    for (const auto& src : sources) {
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
    cropExposureByBlock.emplace(blockPos, 0.0);
}

void GameManager::unregisterCropBlock(const yc::world::BlockPos& blockPos) {
    cropExposureByBlock.erase(blockPos);
}

double GameManager::getCropExposureAtBlock(const yc::world::BlockPos& blockPos) const {
    auto it = cropExposureByBlock.find(blockPos);
    return (it != cropExposureByBlock.end()) ? it->second : 0.0;
}

float GameManager::getCropHealthPercent() const {
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

void GameManager::updateCropExposure(double simDtSec) {
    if (!world || simDtSec <= 0.0 || cropExposureByBlock.empty()) return;

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
            ps.setSource(src);
            total += ps.concentrationAt(worldPos);
        }

        exposure += total * simDtSec;
        dirtyChunks.insert(yc::world::World::GetChunkCoordOf(blockPos));
    }

    for (const auto& pos : toRemove) {
        cropExposureByBlock.erase(pos);
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

    if (world) {
        world->setSettings(settings.world);
        world->setExposureScale(settings.exposure.exposureScale);
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