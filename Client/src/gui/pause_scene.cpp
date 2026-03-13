#include "gui/pause_scene.h"
#include "application.h"
#include <imgui.h>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

namespace yc::gui {

PauseScene::PauseScene(Application* application):
    application(application) {
}

void PauseScene::setPage(PausePage page) {
    activePage = page;
}

void PauseScene::syncWindPathBuffer() {
    if (!application) {
        return;
    }

    auto& settings = application->getSettings();
    if (!windCsvPathBufferInitialized) {
        std::memset(windCsvPathBuffer.data(), 0, windCsvPathBuffer.size());
        strcpy_s(windCsvPathBuffer.data(), windCsvPathBuffer.size(), settings.game.windCsvPath.c_str());
        windCsvPathBufferInitialized = true;
    }
}

void PauseScene::renderMainPage() {
    const float uiScale = std::clamp(application ? application->getSettings().ui.scale : 1.0f, 0.75f, 2.0f);
    const ImVec2 menuButtonSize(260.0f * uiScale, 36.0f * uiScale);
    auto* player = application ? application->getPlayer() : nullptr;
    const auto drawCenteredButton = [&](const char* label) -> bool {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float xOffset = std::max((availableWidth - menuButtonSize.x) * 0.5f, 0.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        return ImGui::Button(label, menuButtonSize);
    };

    if (drawCenteredButton("Resume")) {
        application->resumeGame();
    }

    if (player) {
        const char* movementLabel = player->isFlyMode() ? "Mode: Fly" : "Mode: Walk";
        if (drawCenteredButton(movementLabel)) {
            player->toggleFlyMode();
        }
    }

    if (drawCenteredButton("Skip To Harvest Day")) {
        application->getGameManager().skipToEndOfCurrentSeason();
        application->resumeGame();
    }

    if (drawCenteredButton("Save Select")) {
        application->setSaveSelectionActive(true);
    }

    if (drawCenteredButton("Quit")) {
        application->stop();
    }
}

void PauseScene::renderChimneyPage() {
    auto* world = application ? application->getWorld() : nullptr;
    auto* player = application ? application->getPlayer() : nullptr;
    auto& manager = application->getGameManager();

    if (!world) {
        ImGui::Text("World unavailable");
        return;
    }

    auto chimneySettings = manager.getChimneySettings();
    bool settingsChanged = false;
    const double chimneyExitVelocityMin = 0.1;
    const double chimneyExitVelocityMax = 100.0;

    settingsChanged |= ImGui::SliderInt("Height", &chimneySettings.height, 1, 64);
    settingsChanged |= ImGui::SliderInt("Radius", &chimneySettings.radius, 1, 16);
    settingsChanged |= ImGui::SliderScalar("Exit Velocity", ImGuiDataType_Double, &chimneySettings.exitVelocity, &chimneyExitVelocityMin, &chimneyExitVelocityMax, "%.2f");

    if (settingsChanged) {
        manager.setChimneySettings(chimneySettings);
    }

    const bool canAddChimney = player && player->isSelectingBlock();
    if (!canAddChimney) {
        ImGui::Text("Look at a block, then press Add Chimney.");
    }

    if (ImGui::Button("Add Chimney At Target") && canAddChimney) {
        world->spawnChimneyAt(
            player->getSelectingBlock() + player->getSelectingFace(),
            chimneySettings.height,
            chimneySettings.radius,
            chimneySettings.exitVelocity);
    }

    ImGui::Separator();

    auto& chimneys = world->getChimneyEmittersMutable();
    ImGui::Text("Tracked Chimneys: %d", static_cast<int>(chimneys.size()));

    for (size_t i = 0; i < chimneys.size(); ++i) {
        auto& c = chimneys[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::Text("Chimney %d  Base(%d, %d, %d)", static_cast<int>(i + 1), c.baseBlockCoord.x, c.baseBlockCoord.y, c.baseBlockCoord.z);
        ImGui::SameLine();

        bool enabled = c.enabled;
        if (ImGui::Checkbox("Running", &enabled)) {
            world->setChimneyEmitterEnabledAt(i, enabled);
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            world->removeChimneyEmitterAt(i);
            ImGui::PopID();
            break;
        }

        ImGui::Text("h=%.1f r=%.1f vel=%.1f", c.height, c.radius, c.exitVelocity);
        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button("Back")) {
        activePage = PausePage::Main;
    }
}

void PauseScene::renderSettingsPage() {
    auto& settings = application->getSettings();
    auto& manager = application->getGameManager();
    settings.game.currentSeason = static_cast<int>(manager.getCurrentSeason());
    settings.game.startHour = manager.getTimeOfDay().hours;
    syncWindPathBuffer();

    bool changed = false;
    const double timeScaleMin = 1.0;
    const double timeScaleMax = 100.0;
    const double defaultChimneyExitMin = 0.1;
    const double defaultChimneyExitMax = 100.0;

    if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::Checkbox("Day/Night Cycle Enabled", &settings.game.dayNightCycleEnabled);
        changed |= ImGui::Checkbox("Start Simulation Running", &settings.game.startSimulationRunning);

        double fineTimeScale = std::clamp(settings.game.timeScale, timeScaleMin, timeScaleMax);
        if (ImGui::SliderScalar("Time Scale (Fine)", ImGuiDataType_Double, &fineTimeScale, &timeScaleMin, &timeScaleMax, "%.2f")) {
            settings.game.timeScale = fineTimeScale;
            changed = true;
        }
        changed |= ImGui::InputDouble("Time Scale (Exact)", &settings.game.timeScale, 1.0, 10.0, "%.2f");
        settings.game.timeScale = std::max(0.0, settings.game.timeScale);

        changed |= ImGui::SliderFloat("Temperature C", &settings.game.temperatureC, -40.0f, 60.0f, "%.1f");

        static const char* seasonLabels[] = { "Spring", "Summer", "Autumn", "Winter" };
        int seasonIndex = std::clamp(settings.game.currentSeason, 0, 3);
        if (ImGui::Combo("Current Season", &seasonIndex, seasonLabels, IM_ARRAYSIZE(seasonLabels))) {
            settings.game.currentSeason = seasonIndex;
            manager.setCurrentSeason(static_cast<yc::GameManager::Season>(seasonIndex), true);
            changed = true;
        }

        int hour = std::clamp(settings.game.startHour, 0, 23);
        if (ImGui::SliderInt("Current Hour", &hour, 0, 23)) {
            settings.game.startHour = hour;
            manager.setCurrentHour(hour);
            changed = true;
        }

        ImGui::InputText("Wind CSV Path", windCsvPathBuffer.data(), windCsvPathBuffer.size());
        if (ImGui::Button("Apply Wind CSV Path")) {
            const std::string newPath = windCsvPathBuffer.data();
            if (newPath.empty()) {
                windCsvPathError = "Path cannot be empty.";
            } else if (!fs::exists(newPath)) {
                windCsvPathError = "File does not exist.";
            } else {
                windCsvPathError.clear();
                settings.game.windCsvPath = newPath;
                application->getGameManager().loadWindCsv(newPath);
                changed = true;
            }
        }
        if (!windCsvPathError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", windCsvPathError.c_str());
        }

        const auto date = manager.getDate();
        const auto tod = manager.getTimeOfDay();
        ImGui::Text("Date Preview: Year %d, %s Day %d", date.year, yc::GameManager::getSeasonName(date.season), date.dayOfSeason);
        ImGui::Text("Clock Preview: %02d:%02d:%02d (%s)", tod.hours, tod.minutes, tod.seconds, date.isDaytime ? "Day" : "Night");
    }

    if (ImGui::CollapsingHeader("Interface", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderFloat("UI Scale", &settings.ui.scale, 0.75f, 2.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderFloat("Move Speed", &settings.player.moveSpeed, 1.0f, 200.0f, "%.1f");
        changed |= ImGui::SliderFloat("Gravity Multiplier", &settings.player.gravityMultiplier, 0.1f, 5.0f, "%.2f");
        changed |= ImGui::SliderFloat("Jump Height (Blocks)", &settings.player.jumpHeight, 0.5f, 4.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderFloat("FOV", &settings.camera.fovDeg, 30.0f, 120.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderInt("View Distance", &settings.world.viewDistance, 2, 64);
        changed |= ImGui::SliderInt("Max Unload/Frame", &settings.world.maxUnloadChunkPerFrame, 0, 64);
        changed |= ImGui::SliderInt("Max Load/Frame", &settings.world.maxChunksLoadPerFrame, 0, 64);
        changed |= ImGui::Checkbox("Wireframe World", &settings.world.wireframeMode);
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderFloat("Exposure Scale", &settings.exposure.exposureScale, 0.00001f, 0.1f, "%.5f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Sun Dir X", &settings.lighting.sunDirectionX, -1.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Sun Dir Y", &settings.lighting.sunDirectionY, -1.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Sun Dir Z", &settings.lighting.sunDirectionZ, -1.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Ambient Night", &settings.lighting.ambientNight, 0.0f, 1.5f, "%.3f");
        changed |= ImGui::SliderFloat("Ambient Day", &settings.lighting.ambientDay, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Diffuse Night", &settings.lighting.diffuseNight, 0.0f, 1.5f, "%.3f");
        changed |= ImGui::SliderFloat("Diffuse Day", &settings.lighting.diffuseDay, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Specular Night", &settings.lighting.specularNight, 0.0f, 1.5f, "%.3f");
        changed |= ImGui::SliderFloat("Specular Day", &settings.lighting.specularDay, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Shininess", &settings.lighting.shininess, 1.0f, 128.0f, "%.1f");
        changed |= ImGui::SliderFloat("Water Tint R", &settings.lighting.waterTintR, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Water Tint G", &settings.lighting.waterTintG, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Water Tint B", &settings.lighting.waterTintB, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Water Diffuse Mul", &settings.lighting.waterDiffuseMul, 0.0f, 3.0f, "%.3f");
        changed |= ImGui::SliderFloat("Water Specular Mul", &settings.lighting.waterSpecularMul, 0.0f, 5.0f, "%.3f");
        changed |= ImGui::SliderFloat("Water Min Alpha", &settings.lighting.waterMinAlpha, 0.0f, 1.0f, "%.3f");
    }

    if (ImGui::CollapsingHeader("Chimney Defaults", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderInt("Default Height", &settings.chimney.height, 1, 64);
        changed |= ImGui::SliderInt("Default Radius", &settings.chimney.radius, 1, 16);
        changed |= ImGui::SliderScalar("Default Exit Velocity", ImGuiDataType_Double, &settings.chimney.exitVelocity, &defaultChimneyExitMin, &defaultChimneyExitMax, "%.2f");
    }

    if (ImGui::CollapsingHeader("Smoke", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderInt("Step Count", &settings.smoke.stepCount, 8, 256);
        changed |= ImGui::SliderFloat("Density Scale", &settings.smoke.densityScale, 0.0f, 3.0f, "%.3f");
        changed |= ImGui::SliderFloat("Color R", &settings.smoke.colorR, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("Color G", &settings.smoke.colorG, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("Color B", &settings.smoke.colorB, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("Voxel Size", &settings.smoke.voxelSize, 0.1f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Voxel Threshold", &settings.smoke.voxelThreshold, 0.000001f, 0.01f, "%.6f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Dissipation Half-life", &settings.smoke.dissipationHalfLifeSec, 1.0f, 300.0f, "%.1f");
        changed |= ImGui::SliderFloat("Max Render Distance", &settings.smoke.maxRenderDistance, 10.0f, 2000.0f, "%.1f");
        changed |= ImGui::SliderFloat("Wind Smoothing", &settings.smoke.windSmoothingSec, 0.1f, 30.0f, "%.2f");
        changed |= ImGui::SliderFloat("Wind Transition", &settings.smoke.windTransitionSec, 0.1f, 30.0f, "%.2f");
        changed |= ImGui::SliderFloat("Wind Speed Variation", &settings.smoke.windSpeedVariation, 0.0f, 2.0f, "%.3f");
        changed |= ImGui::SliderFloat("Wind Dir Variation Deg", &settings.smoke.windDirVariationDeg, 0.0f, 45.0f, "%.2f");
        changed |= ImGui::SliderFloat("Wind Variation Scale", &settings.smoke.windVariationScale, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Downwind Fade", &settings.smoke.downwindFade, 1.0f, 400.0f, "%.1f");
        changed |= ImGui::SliderFloat("Box Downwind", &settings.smoke.boxDownwind, 10.0f, 1000.0f, "%.1f");
        changed |= ImGui::SliderFloat("Box Crosswind", &settings.smoke.boxCrosswind, 5.0f, 500.0f, "%.1f");
        changed |= ImGui::SliderFloat("Box Vertical", &settings.smoke.boxVertical, 5.0f, 500.0f, "%.1f");
    }

    if (changed) {
        application->applyCurrentSettings();
    }

    if (ImGui::Button("Back")) {
        activePage = PausePage::Main;
    }
}

void PauseScene::render() {

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.7));
    ImGui::Begin("pause", NULL,
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoSavedSettings);

    ImGui::End();
    ImGui::PopStyleColor();
    
    const auto io = ImGui::GetIO();
    const float uiScale = std::clamp(application ? application->getSettings().ui.scale : 1.0f, 0.75f, 2.0f);
    const ImVec2 menuSize(
        std::min(io.DisplaySize.x * 0.9f, 860.0f * uiScale),
        std::min(io.DisplaySize.y * 0.9f, 760.0f * uiScale));

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(menuSize, ImGuiCond_Always);
    ImGui::Begin("pause_menu", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    const ImVec2 tabButtonSize(120.0f * uiScale, 32.0f * uiScale);
    if (ImGui::Button("Main", tabButtonSize)) {
        activePage = PausePage::Main;
    }
    ImGui::SameLine();
    if (ImGui::Button("Chimneys", tabButtonSize)) {
        activePage = PausePage::ChimneyManager;
    }
    ImGui::SameLine();
    if (ImGui::Button("Settings", tabButtonSize)) {
        activePage = PausePage::Settings;
    }

    ImGui::Separator();
    ImGui::BeginChild("pause_content", ImVec2(0.0f, 0.0f), false);

    switch (activePage) {
    case PausePage::Main:
        renderMainPage();
        break;
    case PausePage::ChimneyManager:
        renderChimneyPage();
        break;
    case PausePage::Settings:
        renderSettingsPage();
        break;
    }

    ImGui::EndChild();

    ImGui::End();
}

}