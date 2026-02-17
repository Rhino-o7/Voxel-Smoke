#include "world/pollution_field_cache.h"
#include "world/world.h"

#include <algorithm>
#include <cmath>

namespace yc::world {

double PollutionFieldCache::hash01(int x, int y, int z, int seed) {
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
    mix(static_cast<uint32_t>(x));
    mix(static_cast<uint32_t>(y));
    mix(static_cast<uint32_t>(z));
    mix(static_cast<uint32_t>(seed));
    return (h & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
}

void PollutionFieldCache::update(float dt) {
    if (dt <= 0.0f) return;

    advect(dt);
    decay(dt);
    addModelSourceTerm(dt);

    if (density.size() > static_cast<size_t>(settings.maxTrackedCells)) {
        size_t toErase = density.size() - static_cast<size_t>(settings.maxTrackedCells);
        for (auto it = density.begin(); it != density.end() && toErase > 0; ) {
            it = density.erase(it);
            --toErase;
        }
    }
}

double PollutionFieldCache::sample(const glm::dvec3& worldPos) const {
    const glm::ivec3 b = World::getWorldtoBlockCoord(worldPos);
    auto it = density.find(b);
    return (it != density.end()) ? it->second : 0.0;
}

void PollutionFieldCache::advect(float dt) {
    if (density.empty()) return;

    // Semi-Lagrangian advection on a sparse set:
    // For each occupied cell, move its density by fractional displacement using 2D bilinear + optional Y rise.
    //
    // This avoids integer "teleport" pops when wind direction changes.

    const glm::dvec2 dirXZ = wind.unitDirXZ();

    const double dx = dirXZ.x * wind.speed * dt * settings.transportScale;
    const double dz = dirXZ.y * wind.speed * dt * settings.transportScale;
    const double dy = settings.risePerSec * dt;

    if (std::abs(dx) < 1e-9 && std::abs(dy) < 1e-9 && std::abs(dz) < 1e-9) return;

    next.clear();

    const float diffuse = settings.diffusion;
    const float keep = 1.0f - diffuse;

    for (const auto& [cell, d0] : density) {
        if (d0 <= 0.001f) continue;

        // Continuous destination position
        const double nx = static_cast<double>(cell.x) + dx;
        const double ny = static_cast<double>(cell.y) + dy;
        const double nz = static_cast<double>(cell.z) + dz;

        const int x0 = static_cast<int>(std::floor(nx));
        const int y0 = static_cast<int>(std::floor(ny));
        const int z0 = static_cast<int>(std::floor(nz));

        const double fx = nx - static_cast<double>(x0);
        const double fy = ny - static_cast<double>(y0);
        const double fz = nz - static_cast<double>(z0);

        // Clamp Y range (skip if entirely out)
        if (y0 < -1 || y0 >= 256) continue;

        // Trilinear scatter to 8 neighbors
        for (int ox = 0; ox <= 1; ++ox) {
            const double wx = (ox == 0) ? (1.0 - fx) : fx;
            for (int oy = 0; oy <= 1; ++oy) {
                const double wy = (oy == 0) ? (1.0 - fy) : fy;
                const int yy = y0 + oy;
                if (yy < 0 || yy >= 256) continue;

                for (int oz = 0; oz <= 1; ++oz) {
                    const double wz = (oz == 0) ? (1.0 - fz) : fz;

                    const float w = static_cast<float>(wx * wy * wz);
                    if (w <= 0.0f) continue;

                    const glm::ivec3 dst{x0 + ox, yy, z0 + oz};

                    // primary deposit
                    next[dst] += d0 * keep * w;

                    // optional cheap lateral diffusion (smears a bit, reduces blocky look)
                    if (diffuse > 1e-6f) {
                        const float share = d0 * (diffuse * 0.25f) * w;
                        next[dst + glm::ivec3( 1, 0, 0)] += share;
                        next[dst + glm::ivec3(-1, 0, 0)] += share;
                        next[dst + glm::ivec3( 0, 0, 1)] += share;
                        next[dst + glm::ivec3( 0, 0,-1)] += share;
                    }
                }
            }
        }
    }

    density.swap(next);
}

void PollutionFieldCache::decay(float dt) {
    if (density.empty()) return;

    const float dec = settings.decayPerSec * dt;

    for (auto it = density.begin(); it != density.end(); ) {
        float& d = it->second;
        d = std::max(0.0f, d - dec);
        if (d <= 0.001f) it = density.erase(it);
        else ++it;
    }
}

void PollutionFieldCache::addModelSourceTerm(float dt) {
    if (!instantSampler) return;
    if (sources.empty()) return;
    if (wind.speed <= 1e-6) return;

    const glm::dvec2 dir = wind.unitDirXZ();
    const glm::dvec2 perp(-dir.y, dir.x);

    int samplesLeft = settings.sourceMaxSamplesPerUpdate;
    const int seed = 12345;

    for (const auto& src : sources) {
        if (samplesLeft <= 0) break;

        const glm::dvec3 origin = src.worldPos + glm::dvec3(0.0, src.height, 0.0);

        for (int i = 0; i <= settings.sourceMaxDownwindBlocks && samplesLeft > 0; ++i) {
            const glm::dvec3 center = origin + glm::dvec3(dir.x * i, 0.0, dir.y * i);

            for (int dw = -settings.sourceMaxCrosswindRadiusBlocks; dw <= settings.sourceMaxCrosswindRadiusBlocks && samplesLeft > 0; ++dw) {
                for (int dy = -settings.sourceMaxVerticalRadiusBlocks; dy <= settings.sourceMaxVerticalRadiusBlocks && samplesLeft > 0; ++dy) {
                    const glm::dvec3 p = center + glm::dvec3(perp.x * dw, dy, perp.y * dw);
                    const glm::ivec3 b = World::getWorldtoBlockCoord(p);

                    if (settings.sourcePatchiness > 1e-6f) {
                        const double n = hash01(b.x, b.y, b.z, seed);
                        if (n < settings.sourcePatchiness) { --samplesLeft; continue; }
                    }

                    const double c = instantSampler(p);
                    --samplesLeft;

                    if (c <= 0.0) continue;

                    float& d = density[b];
                    d = std::min(settings.maxDensity, d + static_cast<float>(c) * settings.sourceGain * dt);
                }
            }
        }
    }
}

}