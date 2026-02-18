#pragma once

#include <string>

namespace yc {

struct Settings {
    struct ExposureSettings {
        float exposureScale = 0.01f;
    } exposure;

    struct GameSettings {
        double timeScale = 10.0;
        float temperatureC = 20.0f;
        bool startSimulationRunning = false;
        std::string windCsvPath = "resources/simulation/wind_test.csv";
    } game;

    struct WorldSettings {
        int viewDistance = 24;
        int maxUnloadChunkPerFrame = 4;
        int maxChunksLoadPerFrame = 2;
    } world;

    struct ChimneySettings {
        int height = 20;
        int radius = 4;
        double exitVelocity = 10.0;
    } chimney;

    struct SmokeSettings {
        double updateIntervalSec = 0.40;
        int maxDownwindBlocks = 96;
        int maxCrosswindRadiusBlocks = 12;
        int maxVerticalRadiusBlocks = 10;
        int maxBlocksPerUpdate = 2500;
        double concentrationThreshold = 0.25;
        double patchiness = 0.20;
    } smoke;
};

}