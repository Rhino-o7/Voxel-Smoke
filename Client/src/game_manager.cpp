#include "game_manager.h"

#include <algorithm>

namespace yc {

GameManager::GameManager() {
    clock.setTimeScale(10.0); // 1s real = 10s game
}

void GameManager::init(yc::world::World* world, Player* player) {
    this->world = world;
    this->player = player;

    if (world) {
        smokeViz = std::make_unique<yc::world::SmokeVisualizer>(world);
        world->setWindState(currentWindState);
    }
}

void GameManager::update(double realDtSec) {
    if (simulationRunning) {
        clock.tick(realDtSec);
        wind.update(clock.getSimTimeSec());
        currentWindState = wind.current();
    }

    pollution.setWind(currentWindState);

    if (world) {
        world->setWindState(currentWindState);
    }
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

}