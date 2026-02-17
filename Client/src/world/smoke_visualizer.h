#pragma once

#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "world/wind_system.h"
#include "world/pollution_system.h"

namespace yc::world {

class World;

class SmokeVisualizer {
public:
    using ConcentrationSampler = std::function<double(const glm::dvec3& worldPos)>;

    struct Settings {
        // How often to rewrite blocks (performance knob)
        double updateIntervalSec = 0.40;

        // Sampling bounds (visual-only sampling domain)
        int maxDownwindBlocks = 96;
        int maxCrosswindRadiusBlocks = 12;
        int maxVerticalRadiusBlocks = 10;

        // Hard cap on blocks written per update
        int maxBlocksPerUpdate = 2500;

        // Threshold for drawing AIR_SMOKE
        double concentrationThreshold = 0.25; // this now maps to "field density", not raw Gaussian C

        // Optional patchiness (skip some samples)
        double patchiness = 0.20;
    };

    explicit SmokeVisualizer(World* world);

    void setSettings(const Settings& s) { settings = s; }
    const Settings& getSettings() const { return settings; }

    // Visuals sample simulation ONLY through sampler
    void update(
        double simTimeSec,
        const WindState& windState,
        const std::vector<ChimneySource>& sources,
        const ConcentrationSampler& sampler);

private:
    World* world = nullptr;
    Settings settings{};
    double nextUpdateSec = 0.0;

    // Track what we placed last time so we can clear it safely
    std::vector<glm::ivec3> smokeBlocks;

    void clearSmokeBlocks();
    static double hash01(int x, int y, int z, int seed);
};

}