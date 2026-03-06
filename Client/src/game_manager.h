#pragma once

#include <string>
#include <cmath>
#include <algorithm>

#include "game_clock.h"
#include "player.h"
#include "world/world.h"
#include "world/wind_system.h"
#include "world/pollution_system.h"
#include "settings.h"

namespace yc {

class GameManager {
public:
    enum class Season {
        Spring = 0,
        Summer,
        Autumn,
        Winter
    };

    struct Date {
        int year = 1;
        int dayOfYear = 1;
        int dayOfSeason = 1;
        Season season = Season::Spring;
        bool isDaytime = true;
    };

    struct HarvestResult {
        bool hasResult = false;
        int cropCount = 0;
        int producedCrops = 0;
        int maxCrops = 0;
    };

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
    Date getDate() const;
    Season getCurrentSeason() const;
    static const char* getSeasonName(Season season);
    bool isDayNightCycleEnabled() const { return dayNightCycleEnabled; }
    void setDayNightCycleEnabled(bool value) { dayNightCycleEnabled = value; }
    void setCurrentSeason(Season season, bool keepTimeOfDay = true);
    void setCurrentHour(int hour);

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
    void registerLoadedCropBlocks();
    void scheduleLoadedCropRegistration(int ticks = 180);
    double getCropExposureAtBlock(const yc::world::BlockPos& blockPos) const;
    yc::world::World* getWorld() const { return world; }
    const yc::world::CropExposureMap& getCropExposureByBlock() const { return cropExposureByBlock; }
    void setCropExposureByBlock(const yc::world::CropExposureMap& map) { cropExposureByBlock = map; }
    int getCropBlockCount() const { return static_cast<int>(cropExposureByBlock.size()); }
    float getCropHealthPercent() const;
    float getCropScoreAtBlock(const yc::world::BlockPos& blockPos) const;
    HarvestResult getHarvestResult() const { return harvestResult; }
    bool isGameOver() const { return gameOver; }
    void resetRound();
    void continueToNextFarmingYear();
    void skipToEndOfCurrentSeason();
    int getFarmingYear() const { return farmingYear; }
    void setFarmingYear(int value) { farmingYear = std::max(1, value); }

    double getTimeScale() const { return clock.getTimeScale(); }
    float getExposureScale() const { return exposureScale; }
    void setTimeScale(double value) { clock.setTimeScale(value); }
    void setSimTimeSec(double value) { clock.setSimTimeSec(value); }
    void setExposureScale(float value) { exposureScale = value; }
    void setSimulationRunning(bool value);

private:
    static constexpr int HoursPerDay = 24;
    static constexpr int DaysPerSeason = 30;
    static constexpr int SeasonsPerYear = 4;
    static constexpr int DaysPerYear = DaysPerSeason * SeasonsPerYear;

    static int getSeasonStartDay(Season season);
    static int positiveMod(int value, int mod);
    void updateCalendarState();
    void finishHarvestIfNeeded();
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
    bool dayNightCycleEnabled = true;
    int lastAbsoluteDay = -1;
    int pendingCropRegistrationTicks = 0;
    int farmingYear = 1;
    bool gameOver = false;
    HarvestResult harvestResult{};
};

}