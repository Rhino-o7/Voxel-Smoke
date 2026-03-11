#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "gl/shader.h"
#include "settings.h"

namespace yc::graphic::lighting {

struct LightingState {
    glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.25f));
    float daylight = 1.0f;
    float ambientStrength = 0.5f;
    float diffuseStrength = 0.65f;
    float specularStrength = 0.1f;
    float shininess = 16.0f;
};

inline float ComputeDaylight(double simTimeSec) {
    constexpr double SecondsPerDay = 24.0 * 3600.0;
    const double daySeconds = std::fmod(simTimeSec, SecondsPerDay);
    const double wrappedDaySeconds = (daySeconds < 0.0) ? (daySeconds + SecondsPerDay) : daySeconds;
    const float hour = static_cast<float>(wrappedDaySeconds / 3600.0);

    constexpr float sunriseStart = 6.0f;
    constexpr float dayStart = 8.0f;
    constexpr float dayEnd = 18.0f;
    constexpr float nightStart = 20.0f;

    if (hour < sunriseStart || hour >= nightStart) {
        return 0.0f;
    }

    if (hour < dayStart) {
        const float duration = std::max(dayStart - sunriseStart, 0.01f);
        return (hour - sunriseStart) / duration;
    }

    if (hour < dayEnd) {
        return 1.0f;
    }

    const float duration = std::max(nightStart - dayEnd, 0.01f);
    return 1.0f - ((hour - dayEnd) / duration);
}

inline LightingState BuildLightingState(double simTimeSec, const yc::Settings::LightingSettings& settings) {
    LightingState state{};
    state.daylight = std::clamp(ComputeDaylight(simTimeSec), 0.0f, 1.0f);

    glm::vec3 sunDirection(settings.sunDirectionX, settings.sunDirectionY, settings.sunDirectionZ);
    if (glm::length(sunDirection) <= 0.0001f) {
        sunDirection = glm::vec3(-0.4f, -1.0f, -0.25f);
    }

    state.sunDirection = glm::normalize(sunDirection);
    state.ambientStrength = std::clamp(settings.ambientNight + ((settings.ambientDay - settings.ambientNight) * state.daylight), 0.0f, 2.0f);
    state.diffuseStrength = std::clamp(settings.diffuseNight + ((settings.diffuseDay - settings.diffuseNight) * state.daylight), 0.0f, 2.0f);
    state.specularStrength = std::clamp(settings.specularNight + ((settings.specularDay - settings.specularNight) * state.daylight), 0.0f, 2.0f);
    state.shininess = std::clamp(settings.shininess, 1.0f, 256.0f);

    return state;
}

inline void ApplyLightingUniforms(yc::gl::Shader& shader, const LightingState& state, const glm::vec3& viewPos) {
    shader.setVec3("uLighting.sunDirection", state.sunDirection);
    shader.setFloat("uLighting.ambientStrength", state.ambientStrength);
    shader.setFloat("uLighting.diffuseStrength", state.diffuseStrength);
    shader.setFloat("uLighting.specularStrength", state.specularStrength);
    shader.setFloat("uLighting.shininess", state.shininess);
    shader.setFloat("uLighting.daylight", state.daylight);
    shader.setVec3("uViewPos", viewPos);
}

}
