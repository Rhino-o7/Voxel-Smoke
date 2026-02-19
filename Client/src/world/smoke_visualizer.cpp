#include "world/smoke_visualizer.h"

namespace yc::world {

SmokeVisualizer::SmokeVisualizer(World* world) : world(world) {}

void SmokeVisualizer::update(
    double simTimeSec,
    const WindState& windState,
    const std::vector<ChimneySource>& sources,
    const ConcentrationSampler& sampler) {
    (void)simTimeSec;
    (void)windState;
    (void)sources;
    (void)sampler;
}

}