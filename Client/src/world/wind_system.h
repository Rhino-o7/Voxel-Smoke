#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace yc::world {

struct WindSample {
    double timestampSec = 0.0;     // monotonically increasing
    double speed = 0.0;            // units/sec (or m/s if you treat voxels as meters)
    double directionDeg = 0.0;     // meteorological or math? here: math degrees, 0=+X, 90=+Z
};

struct WindState {
    double speed = 0.0;
    double directionDeg = 0.0;

    glm::dvec2 unitDirXZ() const;
};

class WindSystem {
public:
    bool loadFromCsvFile(const std::string& path); // timestampSec,speed,directionDeg
    void clear();

    // Advances "simulation clock"; wind becomes the last sample with timestamp <= simTimeSec.
    void update(double simTimeSec);

    const WindState& current() const { return currentState; }
    bool hasData() const { return !samples.empty(); }

private:
    std::vector<WindSample> samples;
    size_t nextIndex = 0;
    WindState currentState{};
};

}