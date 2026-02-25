#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "gl/common.h"
#include "camera.h"
#include "settings.h"
#include "world/wind_system.h"
#include "world/pollution_system.h"

namespace yc::graphic {

class SmokeVolumeRenderer {
public:
    void init();
    void render(yc::Camera* camera,
        const yc::world::WindState& wind,
        const std::vector<yc::world::ChimneySource>& sources,
        const yc::Settings::SmokeSettings& settings,
        double simTimeSec);

private:
    static const float Vertices[];
    GLuint vao = 0;
    GLuint vbo = 0;

    bool computePlumeBounds(const yc::world::ChimneySource& source,
        double windSpeed,
        const glm::dvec2& windDir,
        double downwindMax,
        const yc::Settings::SmokeSettings& settings,
        glm::vec3& outMin,
        glm::vec3& outMax) const;

    glm::vec2 smoothedWindDir{ 1.0f, 0.0f };
    float smoothedWindSpeed = 0.0f;
    glm::vec2 prevWindDir{ 1.0f, 0.0f };
    float prevWindSpeed = 0.0f;
    glm::vec2 lastInputWindDir{ 1.0f, 0.0f };
    float lastInputWindSpeed = 0.0f;
    double windChangeTimeSec = 0.0;
    double lastSimTimeSec = 0.0;
    bool hasWind = false;
    float windNoiseSeed = 0.0f;
};

}
