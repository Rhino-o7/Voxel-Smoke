#include "world/smoke_visualizer.h"
#include "world/world.h"
#include "world/block.h"

#include <cmath>
#include <algorithm>

namespace yc::world {

SmokeVisualizer::SmokeVisualizer(World* world) : world(world) {}

double SmokeVisualizer::hash01(int x, int y, int z, int seed) {
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
    mix(static_cast<uint32_t>(x));
    mix(static_cast<uint32_t>(y));
    mix(static_cast<uint32_t>(z));
    mix(static_cast<uint32_t>(seed));
    return (h & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
}

void SmokeVisualizer::clearSmokeBlocks() {
    for (const auto& b : smokeBlocks) {
        const BlockData existing = world->getBlockDataIfLoadedAt(b);
        if (existing.getType() == BlockType::AIR_SMOKE) {
            world->setBlockDataIfLoadedAt(b, { BlockType::AIR });
        }
    }
    smokeBlocks.clear();
}

void SmokeVisualizer::update(
    double simTimeSec,
    const WindState& windState,
    const std::vector<ChimneySource>& sources,
    const ConcentrationSampler& sampler) {

    if (!sampler) return;

    if (simTimeSec < nextUpdateSec) return;
    nextUpdateSec = simTimeSec + settings.updateIntervalSec;

    clearSmokeBlocks();

    // If no wind, still visualize around sources locally (optional)
    const glm::dvec2 dir = windState.unitDirXZ();
    const glm::dvec2 perp(-dir.y, dir.x);

    int budget = settings.maxBlocksPerUpdate;
    const int tSeed = static_cast<int>(simTimeSec * 0.5);

    for (const auto& src : sources) {
        if (budget <= 0) break;

        // Sample the current pollution FIELD around the source and downwind.
        // IMPORTANT: this uses only sampler(p). No model knowledge here.
        const glm::dvec3 base = src.worldPos + glm::dvec3(0.0, src.height, 0.0);

        for (int i = 0; i <= settings.maxDownwindBlocks && budget > 0; ++i) {
            const glm::dvec3 center = base + glm::dvec3(dir.x * i, 0.0, dir.y * i);

            for (int dw = -settings.maxCrosswindRadiusBlocks; dw <= settings.maxCrosswindRadiusBlocks && budget > 0; ++dw) {
                for (int dy = -settings.maxVerticalRadiusBlocks; dy <= settings.maxVerticalRadiusBlocks && budget > 0; ++dy) {
                    const glm::dvec3 p = center + glm::dvec3(perp.x * dw, dy, perp.y * dw);

                    const glm::ivec3 b = World::getWorldtoBlockCoord(p);

                    // patchiness (perf + cloudiness)
                    if (settings.patchiness > 1e-6) {
                        const double n = hash01(b.x, b.y, b.z, tSeed);
                        if (n < settings.patchiness) continue;
                    }

                    const double v = sampler(p); // <- ONLY input
                    if (v < settings.concentrationThreshold) continue;

                    const BlockData existing = world->getBlockDataIfLoadedAt(b);
                    if (existing.getType() != BlockType::AIR) continue;

                    if (world->setBlockDataIfLoadedAt(b, { BlockType::AIR_SMOKE })) {
                        smokeBlocks.push_back(b);
                        --budget;
                    }
                }
            }
        }
    }
}

}