#pragma once

#include <memory>
#include <string>
#include <cmath>

#include "game_clock.h"
#include "player.h"
#include "world/world.h"
#include "world/wind_system.h"
#include "world/pollution_system.h"
#include "world/smoke_visualizer.h"

namespace yc {

class GameManager {
public:
    struct ChimneySettings {
        int height = 20;
        int radius = 5;
        double exitVelocity = 10.0;
    };

    struct TimeOfDay {
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
    };

    GameManager();

    void init(yc::world::World* world, Player* player);

    void update(double realDtSec);

    void pauseTime();
    void resumeTime();

    void startSimulation();
    void stopSimulation();
    void toggleSimulation();
    bool isSimulationRunning() const { return simulationRunning; }

    bool loadWindCsv(const std::string& path);

    GameClock& getClock() { return clock; }
    const GameClock& getClock() const { return clock; }

    double getSimTimeSec() const { return clock.getSimTimeSec(); }
    TimeOfDay getTimeOfDay() const;

    float getTemperatureC() const { return temperatureC; }
    void setTemperatureC(float value) { temperatureC = value; }

    const yc::world::WindState& getWindState() const { return currentWindState; }

    const ChimneySettings& getChimneySettings() const { return chimneySettings; }
    void setChimneySettings(const ChimneySettings& settings) { chimneySettings = settings; }

    double getPollutionAtWorld(const yc::world::WorldPos& worldPos) const;
    double getPollutionAtBlock(const yc::world::BlockPos& blockPos) const;

private:
    yc::world::World* world = nullptr;
    Player* player = nullptr;

    GameClock clock;
    yc::world::WindSystem wind;
    yc::world::PollutionSystem pollution;

    std::unique_ptr<yc::world::SmokeVisualizer> smokeViz;

    ChimneySettings chimneySettings{};
    float temperatureC = 20.0f;

    bool simulationRunning = false;
    bool resumeOnUnpause = false;
    yc::world::WindState currentWindState{};
};

}