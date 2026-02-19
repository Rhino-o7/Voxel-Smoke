#pragma once

#include <string>

namespace yc {

struct Settings {
    struct ExposureSettings {
        float exposureScale = 0.001f;
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
        int stepCount = 64;
        float densityScale = 0.45f;
        float colorR = 0.5f;
        float colorG = 0.7f;
        float colorB = 0.4f;

        float boxDownwind = 200.0f;
        float boxCrosswind = 50.0f;
        float boxVertical = 50.0f;
    } smoke;
};

}