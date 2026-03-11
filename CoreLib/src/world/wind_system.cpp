#include "world/wind_system.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace yc::world {

static double DegToRad(double deg) {
    return deg * (3.14159265358979323846 / 180.0);
}

glm::dvec2 WindState::unitDirXZ() const {
    // 0 deg => +X, 90 deg => +Z (right-handed XZ plane)
    const double r = DegToRad(directionDeg);
    return glm::dvec2(std::cos(r), std::sin(r));
}

void WindSystem::clear() {
    samples.clear();
    nextIndex = 0;
    currentState = {};
}

bool WindSystem::loadFromCsvFile(const std::string& path) {
    clear();

    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        std::stringstream ss(line);

        std::string ts, spd, dir;
        if (!std::getline(ss, ts, ',')) continue;
        if (!std::getline(ss, spd, ',')) continue;
        if (!std::getline(ss, dir, ',')) continue;

        WindSample s{};
        s.timestampSec = std::stod(ts);
        s.speed = std::stod(spd);
        s.directionDeg = std::stod(dir);
        samples.push_back(s);
    }

    std::sort(samples.begin(), samples.end(), [](const WindSample& a, const WindSample& b) {
        return a.timestampSec < b.timestampSec;
    });

    nextIndex = 0;
    if (!samples.empty()) {
        currentState.speed = samples[0].speed;
        currentState.directionDeg = samples[0].directionDeg;
        nextIndex = 1;
    }

    return !samples.empty();
}

void WindSystem::update(double simTimeSec) {
    if (samples.empty()) return;

    // Advance samples in order; hold last known value.
    while (nextIndex < samples.size() && samples[nextIndex].timestampSec <= simTimeSec) {
        currentState.speed = samples[nextIndex].speed;
        currentState.directionDeg = samples[nextIndex].directionDeg;
        ++nextIndex;
    }
}

}