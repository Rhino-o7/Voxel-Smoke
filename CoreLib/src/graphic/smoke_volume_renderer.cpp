#include "graphic/smoke_volume_renderer.h"

#include <algorithm>
#include <cmath>
#include "resource.h"
#include "glm/gtc/matrix_transform.hpp"

namespace yc::graphic {

const float SmokeVolumeRenderer::Vertices[] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f,

    0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f,

    0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 1.0f,

    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f,

    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 0.0f,

    0.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f
};

void SmokeVolumeRenderer::init() {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(SmokeVolumeRenderer::Vertices), &SmokeVolumeRenderer::Vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

bool SmokeVolumeRenderer::computePlumeBounds(const yc::world::ChimneySource& source,
    double windSpeed,
    const glm::dvec2& windDir,
    double downwindMax,
    const yc::Settings::SmokeSettings& settings,
    glm::vec3& outMin,
    glm::vec3& outMax) const {
    if (windSpeed <= 1e-6) {
        return false;
    }

    if (downwindMax <= 0.01) {
        return false;
    }

    const glm::dvec2 perp(-windDir.y, windDir.x);

    const glm::dvec2 base(source.worldPos.x, source.worldPos.z);
    const double sourceRadius = std::max(source.radius, static_cast<double>(settings.voxelSize));
    const double cross = std::max(static_cast<double>(settings.boxCrosswind), sourceRadius * 1.5);

    const glm::dvec2 p0 = base + perp * cross;
    const glm::dvec2 p1 = base - perp * cross;
    const glm::dvec2 p2 = base + windDir * downwindMax + perp * cross;
    const glm::dvec2 p3 = base + windDir * downwindMax - perp * cross;

    const double minX = std::min({ p0.x, p1.x, p2.x, p3.x });
    const double maxX = std::max({ p0.x, p1.x, p2.x, p3.x });
    const double minZ = std::min({ p0.y, p1.y, p2.y, p3.y });
    const double maxZ = std::max({ p0.y, p1.y, p2.y, p3.y });

    const double x = std::max(downwindMax, 0.1);
    const double vRatio = source.exitVelocity / std::max(1.0, windSpeed);
    const double widen = 1.0 + 0.12 * std::clamp(vRatio, 0.0, 8.0);
    const double sigmaZ = std::max(0.3 * std::sqrt(x) * widen, sourceRadius * 0.12);
    const double verticalPad = std::max(sourceRadius, sigmaZ * 3.0);

    const double minY = source.worldPos.y + source.height - verticalPad;
    const double maxY = source.worldPos.y + source.height + std::max(static_cast<double>(settings.boxVertical), verticalPad);

    if (maxX <= minX || maxY <= minY || maxZ <= minZ) {
        return false;
    }

    outMin = glm::vec3(static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(minZ));
    outMax = glm::vec3(static_cast<float>(maxX), static_cast<float>(maxY), static_cast<float>(maxZ));
    return true;
}

void SmokeVolumeRenderer::render(yc::Camera* camera,
    const yc::world::WindState& wind,
    const std::vector<yc::world::ChimneySource>& sources,
    const yc::Settings::SmokeSettings& settings,
    double simTimeSec) {
    if (!camera || sources.empty()) {
        return;
    }

    if (settings.boxDownwind <= 0.0f || settings.boxCrosswind <= 0.0f || settings.boxVertical <= 0.0f) {
        return;
    }

    const glm::dvec2 windDirTargetD = wind.unitDirXZ();
    glm::vec2 windDirTarget(static_cast<float>(windDirTargetD.x), static_cast<float>(windDirTargetD.y));
    if (glm::length(windDirTarget) <= 1e-6f) {
        windDirTarget = smoothedWindDir;
        if (glm::length(windDirTarget) <= 1e-6f) {
            windDirTarget = glm::vec2(1.0f, 0.0f);
        }
    }

    const double dt = simTimeSec - lastSimTimeSec;
    if (hasWind && dt < 0.0) {
        hasWind = false;
    }
    // If simulation time hasn't advanced (paused or stopped), skip rendering smoke so
    // the visual disappears while paused. Update lastSimTimeSec so when time resumes
    // we start blending from the correct simulation time.
    if (dt <= 0.0) {
        lastSimTimeSec = simTimeSec;
        return;
    }
    const glm::vec2 previousDir = smoothedWindDir;
    const float previousSpeed = smoothedWindSpeed;
    if (!hasWind || dt < 0.0) {
        smoothedWindDir = glm::normalize(windDirTarget);
        smoothedWindSpeed = static_cast<float>(wind.speed);
        prevWindDir = smoothedWindDir;
        prevWindSpeed = smoothedWindSpeed;
        windChangeTimeSec = simTimeSec;
        lastInputWindDir = windDirTarget;
        lastInputWindSpeed = static_cast<float>(wind.speed);
        windNoiseSeed = static_cast<float>(std::fmod(simTimeSec * 0.1234, 1000.0));
        hasWind = true;
    } else if (dt > 0.0) {
        const float tau = std::max(settings.windSmoothingSec, 0.01f);
        const float alpha = 1.0f - std::exp(static_cast<float>(-dt) / tau);
        const glm::vec2 blended = glm::mix(smoothedWindDir, windDirTarget, alpha);
        smoothedWindDir = (glm::length(blended) > 1e-6f) ? glm::normalize(blended) : smoothedWindDir;
        smoothedWindSpeed = glm::mix(smoothedWindSpeed, static_cast<float>(wind.speed), alpha);
    }

    lastSimTimeSec = simTimeSec;

    const float effectiveWindSpeed = std::max(smoothedWindSpeed, 0.25f);

    const float variationScale = std::max(settings.windVariationScale, 0.01f);
    const float variationPhase = static_cast<float>(simTimeSec) * variationScale + windNoiseSeed;
    const float dirJitterRad = settings.windDirVariationDeg * 0.01745329252f * std::sin(variationPhase * 0.73f);
    const float speedJitter = settings.windSpeedVariation * std::sin(variationPhase * 1.11f + 1.7f);

    const glm::vec2 windDirTargetNorm = glm::normalize(windDirTarget);
    const glm::vec2 lastInputNorm = glm::normalize(lastInputWindDir);
    const float dotDir = glm::clamp(glm::dot(lastInputNorm, windDirTargetNorm), -1.0f, 1.0f);
    const float angle = std::acos(dotDir);
    const float speedDelta = std::abs(lastInputWindSpeed - static_cast<float>(wind.speed));
    const float angleThreshold = 8.0f * 0.01745329252f;
    if (angle > angleThreshold || speedDelta > 0.5f) {
        prevWindDir = previousDir;
        prevWindSpeed = previousSpeed;
        windChangeTimeSec = simTimeSec;
        lastInputWindDir = windDirTarget;
        lastInputWindSpeed = static_cast<float>(wind.speed);
    }

    const float jitteredSpeed = std::max(0.0f, effectiveWindSpeed + speedJitter * effectiveWindSpeed);
    const float maxSpeed = std::max(jitteredSpeed, std::max(prevWindSpeed, effectiveWindSpeed));
    const double downwindMax = std::min(static_cast<double>(settings.boxDownwind), static_cast<double>(maxSpeed) * simTimeSec);
    if (downwindMax <= 0.01) {
        return;
    }

    yc::Resource::SmokeVolumeShader.use();
    yc::Resource::SmokeVolumeShader.setMat4("projection_view", camera->getProjectionViewMatrix());
    yc::Resource::SmokeVolumeShader.setVec3("uCameraPos", camera->getPosition());

    const float c = std::cos(dirJitterRad);
    const float s = std::sin(dirJitterRad);
    const glm::vec2 jitteredDir = glm::normalize(glm::vec2(
        smoothedWindDir.x * c - smoothedWindDir.y * s,
        smoothedWindDir.x * s + smoothedWindDir.y * c));

    const glm::dvec2 windDirD(jitteredDir.x, jitteredDir.y);
    const glm::vec2 windDir(jitteredDir.x, jitteredDir.y);

    yc::Resource::SmokeVolumeShader.setFloat("uWindSpeed", jitteredSpeed);
    yc::Resource::SmokeVolumeShader.setVec2("uWindDirXZ", windDir);
    yc::Resource::SmokeVolumeShader.setVec2("uPrevWindDirXZ", prevWindDir);
    yc::Resource::SmokeVolumeShader.setFloat("uPrevWindSpeed", prevWindSpeed);
    yc::Resource::SmokeVolumeShader.setFloat("uWindChangeTimeSec", static_cast<float>(windChangeTimeSec));
    yc::Resource::SmokeVolumeShader.setFloat("uWindTransitionSec", settings.windTransitionSec);
    yc::Resource::SmokeVolumeShader.setInt("uStepCount", settings.stepCount);
    yc::Resource::SmokeVolumeShader.setFloat("uDensityScale", settings.densityScale);
    yc::Resource::SmokeVolumeShader.setVec3("uSmokeColor", glm::vec3(settings.colorR, settings.colorG, settings.colorB));
    yc::Resource::SmokeVolumeShader.setFloat("uSimTimeSec", static_cast<float>(simTimeSec));
    yc::Resource::SmokeVolumeShader.setFloat("uVoxelSize", settings.voxelSize);
    yc::Resource::SmokeVolumeShader.setFloat("uVoxelThreshold", settings.voxelThreshold);
    yc::Resource::SmokeVolumeShader.setFloat("uDissipationHalfLifeSec", settings.dissipationHalfLifeSec);
    yc::Resource::SmokeVolumeShader.setFloat("uMaxDownwind", static_cast<float>(downwindMax));
    yc::Resource::SmokeVolumeShader.setFloat("uDownwindFade", settings.downwindFade);
    yc::Resource::SmokeVolumeShader.setInt("uUseWeightedOIT", 0);

    glBindVertexArray(vao);

    const glm::vec3 cameraPos = camera->getPosition();
    const float maxDist = settings.maxRenderDistance;
    const float maxDistSq = maxDist > 0.0f ? maxDist * maxDist : 0.0f;

    for (const auto& source : sources) {
        if (!source.enabled) {
            continue;
        }

        glm::vec3 boxMin;
        glm::vec3 boxMax;
        if (!computePlumeBounds(source, jitteredSpeed, windDirD, downwindMax, settings, boxMin, boxMax)) {
            continue;
        }

        if (maxDist > 0.0f) {
            const glm::vec3 center = (boxMin + boxMax) * 0.5f;
            const glm::vec3 delta = center - cameraPos;
            const float distSq = glm::dot(delta, delta);
            if (distSq > maxDistSq) {
                continue;
            }
        }

        yc::Resource::SmokeVolumeShader.setVec3("uBoxMin", boxMin);
        yc::Resource::SmokeVolumeShader.setVec3("uBoxMax", boxMax);

        const glm::vec3 size = boxMax - boxMin;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), boxMin) * glm::scale(glm::mat4(1.0f), size);
        yc::Resource::SmokeVolumeShader.setMat4("model", model);

        yc::Resource::SmokeVolumeShader.setVec4("uSourcePosH", glm::vec4(
            static_cast<float>(source.worldPos.x),
            static_cast<float>(source.worldPos.y),
            static_cast<float>(source.worldPos.z),
            static_cast<float>(source.height)));
        yc::Resource::SmokeVolumeShader.setVec4("uSourceParams", glm::vec4(
            static_cast<float>(source.exitVelocity),
            static_cast<float>(source.radius),
            0.0f,
            0.0f));

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindVertexArray(0);
}

}
