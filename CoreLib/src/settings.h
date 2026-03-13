#pragma once

#include <string>

namespace yc {

struct Settings {
    // Global UI scaling for in-game overlays.
    struct UiSettings {
        float scale = 1.0f;
    } ui;

    // Maps accumulated crop exposure to visual/gameplay darkening.
    struct ExposureSettings {
        float exposureScale = 0.001f;
    } exposure;

    // Lighting profile used across terrain, water and day/night transitions.
    struct LightingSettings {
        float sunDirectionX = -0.4f;
        float sunDirectionY = -1.0f;
        float sunDirectionZ = -0.25f;

        float sunriseStartHour = 6.0f;
        float dayStartHour = 8.0f;
        float dayEndHour = 18.0f;
        float nightStartHour = 20.0f;

        float ambientNight = 0.22f;
        float ambientDay = 0.50f;
        float diffuseNight = 0.08f;
        float diffuseDay = 0.65f;
        float specularNight = 0.02f;
        float specularDay = 0.10f;
        float shininess = 16.0f;

        float waterTintR = 0.70f;
        float waterTintG = 0.90f;
        float waterTintB = 1.35f;
        float waterDiffuseMul = 0.75f;
        float waterSpecularMul = 2.4f;
        float waterMinAlpha = 0.72f;
    } lighting;

    // Core simulation startup defaults.
    struct GameSettings {
        double timeScale = 10.0;
        float temperatureC = 20.0f;
        bool startSimulationRunning = false;
        std::string windCsvPath = "resources/simulation/wind.csv";
        int currentSeason = 0;
        bool dayNightCycleEnabled = true;
        int startHour = 8;
    } game;

    // Chunk loading and world render controls.
    struct WorldSettings {
        int viewDistance = 24;
        int maxUnloadChunkPerFrame = 4;
        int maxChunksLoadPerFrame = 2;
        bool wireframeMode = false;
    } world;

    struct CameraSettings {
        float fovDeg = 70.0f;
    } camera;

    struct PlayerSettings {
        float moveSpeed = 50.0f;
        float gravityMultiplier = 1.0f;
        float jumpHeight = 1.0f;
    } player;

    // Default dimensions for newly spawned chimneys.
    struct ChimneySettings {
        int height = 20;
        int radius = 4;
        double exitVelocity = 10.0;
    } chimney;

    // Smoke volume raymarch and plume behavior tuning.
    struct SmokeSettings {
        int stepCount = 128;
        float densityScale = 0.45f;
        float colorR = 0.5f;
        float colorG = 0.7f;
        float colorB = 0.4f;

        float voxelSize = 2.0f;
        float voxelThreshold = 0.0002f;
        float dissipationHalfLifeSec = 30.0f;
        float maxRenderDistance = 500.0f;
        float windSmoothingSec = 4.0f;
        float windTransitionSec = 6.0f;
        float windSpeedVariation = 0.25f;
        float windDirVariationDeg = 6.0f;
        float windVariationScale = 0.1f;
        float downwindFade = 30.0f;

        float boxDownwind = 200.0f;
        float boxCrosswind = 50.0f;
        float boxVertical = 50.0f;
    } smoke;
};

}