#include "gui.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "application.h"

namespace yc::gui {

GUI::GUI():
    gameScene(nullptr),
    pauseScene(nullptr),
    saveSelectScene(nullptr) {

}

GUI::~GUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GUI::init(GLFWwindow* window) {
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 440");

    ImFontConfig config;
    config.SizePixels = 18;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    io.Fonts->AddFontDefault(&config);
}

void GUI::update(yc::Application* application, Player* player, yc::GameManager* gameManager) {
    this->application = application;
    this->gameManager = gameManager;

    if (application && application->isSaveSelectionActive()) {
        if (!saveSelectScene) {
            saveSelectScene = std::make_shared<SaveSelectScene>(application);
        }
    } else {
        saveSelectScene = nullptr;
    }

    if (gameScene == nullptr) {
        gameScene = std::make_shared<GameScene>(player, gameManager);
    }
}

void GUI::pause(yc::Application* application, PausePage page) {
    if (!pauseScene) {
        pauseScene = std::make_shared<PauseScene>(application);
    }
    pauseScene->setPage(page);
}

void GUI::setPausePage(PausePage page) {
    if (pauseScene) {
        pauseScene->setPage(page);
    }
}

void GUI::resume() {
    pauseScene = nullptr;
}

void GUI::render() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (saveSelectScene) {
        saveSelectScene->render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    if (gameScene) {
        gameScene->render();
    }

    if (pauseScene) {
        pauseScene->render();
    }

    if (gameManager && gameManager->isGameOver()) {
        const auto result = gameManager->getHarvestResult();
        const auto date = gameManager->getDate();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.7f));
        ImGui::Begin("game_over_overlay", nullptr,
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
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 340.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("game_over_menu", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoBackground);

        ImGui::Text("Harvest Complete");
        ImGui::Separator();
        ImGui::Text("Year: %d", date.year);
        ImGui::Text("Season Ended: %s", yc::GameManager::getSeasonName(date.season));
        ImGui::Text("Crop Blocks Harvested: %d", result.cropCount);
        ImGui::Text("Crops Produced: %d / %d", result.producedCrops, result.maxCrops);
        ImGui::Spacing();

        const ImVec2 nextYearButtonSize(300.0f, 38.0f);
        if (ImGui::Button("Continue To Next Farming Year", nextYearButtonSize)) {
            gameManager->continueToNextFarmingYear();
        }

        if (ImGui::Button("Load Another Save", nextYearButtonSize)) {
            if (application) {
                application->setSaveSelectionActive(true);
            }
        }

        if (ImGui::Button("Quit", nextYearButtonSize)) {
            if (application) {
                application->stop();
            }
        }

        ImGui::End();
    }

    // --- HUD: Healthbar, Timer, Windspeed ---

    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoTitleBar
                              | ImGuiWindowFlags_NoResize
                              | ImGuiWindowFlags_NoMove
                              | ImGuiWindowFlags_NoScrollbar
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::Begin("HUD", nullptr, hudFlags);

    float healthPercent = gameManager ? gameManager->getCropHealthPercent() : 1.0f;
    const float scorePercent = healthPercent * 100.0f;

    ImGui::Text("Crop Score");
    ImVec2 barSize(200.0f, 20.0f);
    ImGui::ProgressBar(healthPercent, barSize);
    ImGui::Text("Average: %.1f / 100", scorePercent);
    ImGui::Spacing();

    if (gameManager) {
        const auto tod = gameManager->getTimeOfDay();
        const auto date = gameManager->getDate();
        ImGui::Text("Date: Year %d, %s Day %d", date.year, yc::GameManager::getSeasonName(date.season), date.dayOfSeason);
        ImGui::Text("Time: %02d:%02d:%02d (%s)", tod.hours, tod.minutes, tod.seconds, date.isDaytime ? "Day" : "Night");

        ImGui::Text("Sim: %s (T)", gameManager->isSimulationRunning() ? "Running" : "Paused");

        const auto& wind = gameManager->getWindState();
        ImGui::Text("Wind: %.1f m/s", wind.speed);
        if (wind.speed <= 1e-6) {
            ImGui::Text("Dir: --");
        } else {
            ImGui::Text("Dir: %.0f deg", wind.directionDeg);
        }

        ImGui::Text("Temp: %.1f C", gameManager->getTemperatureC());
    } else {
        ImGui::Text("Date: --");
        ImGui::Text("Time: --:--:--");
        ImGui::Text("Sim: -- (T)");
        ImGui::Text("Wind: --.- m/s");
        ImGui::Text("Dir: --");
        ImGui::Text("Temp: --.- C");
    }

    ImGui::End();
    // --- end HUD ---

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}