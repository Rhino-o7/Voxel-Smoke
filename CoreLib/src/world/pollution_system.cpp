#include "world/pollution_system.h"
#include <cmath>
#include <algorithm>

namespace yc::world {

    double PollutionSystem::concentrationAt(const glm::dvec3& p) const {
        // Gaussian plume approximation in wind-aligned coordinates.
        const double windSpeed = windState.speed;
        if (windSpeed <= 1e-6) return 0.0;

        const glm::dvec3 rel = p - src.worldPos;

        // Wind direction in XZ plane
        const glm::dvec2 w = windState.unitDirXZ();
        const glm::dvec2 r(rel.x, rel.z);

        const double downwind = r.x * w.x + r.y * w.y;
        if (downwind <= 0.0) return 0.0;

        const glm::dvec2 perp(-w.y, w.x);
        const double crosswind = r.x * perp.x + r.y * perp.y;

        // Centerline height: stack height only (prevents "gap" when exitVelocity is high)
        const double H = src.height;

        const double x = clampMin(downwind, 0.1);

        // Exit velocity influences dispersion (turbulence), not an instant vertical offset.
        // Mildly widen plume when exit velocity is high relative to wind.
        const double vRatio = src.exitVelocity / std::max(1.0, windSpeed);
        const double widen = 1.0 + 0.12 * std::clamp(vRatio, 0.0, 8.0);

        const double sigmaY = 0.6 * std::sqrt(x) * widen; // crosswind spread
        const double sigmaZ = 0.3 * std::sqrt(x) * widen; // vertical spread

        // Emission "strength" proxy (area * exit velocity)
        const double area = 3.14159265358979323846 * src.radius * src.radius;
        const double Q = area * src.exitVelocity;

        const double gy = std::exp(-(crosswind * crosswind) / (2.0 * sigmaY * sigmaY));
        const double z = rel.y;
        const double gz = std::exp(-((z - H) * (z - H)) / (2.0 * sigmaZ * sigmaZ));

        const double dilution = (windSpeed * sigmaY * sigmaZ);
        const double C = (Q / (2.0 * 3.14159265358979323846 * dilution)) * gy * gz;

        return C;
    }
}