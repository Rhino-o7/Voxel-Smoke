#pragma once

#include <glm/glm.hpp>
#include "world/wind_system.h"

namespace yc::world {

struct ChimneySource {
    glm::dvec3 worldPos{};     // base position in world coordinates
    double height = 10.0;      // stack height
    double exitVelocity = 10.0;// units/sec
    double radius = 0.5;       // stack radius (for area)
};

class PollutionSystem {
public:
    void setSource(const ChimneySource& source) { src = source; }
    void setWind(const WindState& wind) { windState = wind; }

    // Returns concentration at world position (arbitrary units).
    double concentrationAt(const glm::dvec3& worldPos) const;

private:
    ChimneySource src{};
    WindState windState{};

    static double clampMin(double v, double minV) { return v < minV ? minV : v; }
};

}