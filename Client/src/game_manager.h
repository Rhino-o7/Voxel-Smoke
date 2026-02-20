#pragma once

#include <string>
#include <cmath>

#include "game_clock.h"
#include "player.h"
#include "world/world.h"
#include "world/wind_system.h"
#include "world/pollution_system.h"
#include "settings.h"

namespace yc {

class GameManager {
public:
    struct ChimneySettings {
        int height = 20;
        int radius = 5;
        double exitVelocity = 10.0;
    };

    enum class ChimneyParam {
        Height,
        Radius,
        ExitVelocity
    };

    struct TimeOfDay {
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
    };

    GameManager();

    void init(yc::world::World* world, Player* player);

    void applySettings(const Settings& settings);

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

    ChimneyParam getActiveChimneyParam() const { return activeChimneyParam; }
    void setActiveChimneyParam(ChimneyParam value) { activeChimneyParam = value; }

    double getPollutionAtWorld(const yc::world::WorldPos& worldPos) const;
    double getPollutionAtBlock(const yc::world::BlockPos& blockPos) const;

    void registerCropBlock(const yc::world::BlockPos& blockPos);
    void unregisterCropBlock(const yc::world::BlockPos& blockPos);
    double getCropExposureAtBlock(const yc::world::BlockPos& blockPos) const;
    const yc::world::CropExposureMap& getCropExposureByBlock() const { return cropExposureByBlock; }
    void setCropExposureByBlock(const yc::world::CropExposureMap& map) { cropExposureByBlock = map; }
    float getCropHealthPercent() const;

    double getTimeScale() const { return clock.getTimeScale(); }
    float getExposureScale() const { return exposureScale; }
    void setTimeScale(double value) { clock.setTimeScale(value); }
    void setSimTimeSec(double value) { clock.setSimTimeSec(value); }
    void setExposureScale(float value) { exposureScale = value; }
    void setSimulationRunning(bool value);

private:
    void updateCropExposure(double simDtSec);

    yc::world::World* world = nullptr;
    Player* player = nullptr;

    GameClock clock;
    yc::world::WindSystem wind;
    yc::world::PollutionSystem pollution;

    ChimneySettings chimneySettings{};
    ChimneyParam activeChimneyParam = ChimneyParam::Height;
    float temperatureC = 20.0f;

    bool simulationRunning = false;
    bool resumeOnUnpause = false;
    yc::world::WindState currentWindState{};
    yc::world::CropExposureMap cropExposureByBlock;
    float exposureScale = 0.01f;
};

}