#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "world/wind_system.h"
#include "world/pollution_system.h"

namespace yc::world {

class PollutionFieldCache {
public:
    struct Settings {
        int maxTrackedCells = 30000;

        float decayPerSec = 0.12f;
        float diffusion = 0.05f;

        float transportScale = 1.0f;
        float risePerSec = 0.02f;

        float maxDensity = 1.5f;

        // density += model(p) * sourceGain * dt
        float sourceGain = 25.0f;

        // Where to sample the model to add it as a source term
        int sourceMaxDownwindBlocks = 96;
        int sourceMaxCrosswindRadiusBlocks = 12;
        int sourceMaxVerticalRadiusBlocks = 10;
        int sourceMaxSamplesPerUpdate = 9000;

        float sourcePatchiness = 0.10f;
    };

    void setSettings(const Settings& s) { settings = s; }

    void setWind(const WindState& ws) { wind = ws; }
    void setSources(const std::vector<ChimneySource>& s) { sources = s; }

    void setInstantModelSampler(std::function<double(const glm::dvec3&)> sampler) { instantSampler = std::move(sampler); }

    void update(float dt);

    // This is what visuals/gameplay should sample
    double sample(const glm::dvec3& worldPos) const;

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const noexcept {
            size_t hx = std::hash<int32_t>{}(v.x);
            size_t hy = std::hash<int32_t>{}(v.y);
            size_t hz = std::hash<int32_t>{}(v.z);
            return hx ^ (hy << 1) ^ (hz << 2);
        }
    };

    Settings settings{};
    WindState wind{};
    std::vector<ChimneySource> sources;
    std::function<double(const glm::dvec3&)> instantSampler;

    std::unordered_map<glm::ivec3, float, IVec3Hash> density;
    std::unordered_map<glm::ivec3, float, IVec3Hash> next;

    glm::dvec3 advectRemainder{0.0, 0.0, 0.0};

private:
    void advect(float dt);
    void decay(float dt);
    void addModelSourceTerm(float dt);

    static double hash01(int x, int y, int z, int seed);
};

}