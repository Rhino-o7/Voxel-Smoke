#include "gui.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace yc::gui {

GUI::GUI():
    gameScene(nullptr),
    pauseScene(nullptr) {

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
    this->gameManager = gameManager;
    if (gameScene == nullptr) {
        gameScene = std::make_shared<GameScene>(player, gameManager);
    }
}

void GUI::pause(yc::Application* application) {
    pauseScene = std::make_shared<PauseScene>(application);
}

void GUI::resume() {
    pauseScene = nullptr;
}

void GUI::render() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    if (gameScene) {
        gameScene->render();
    }

    if (pauseScene) {
        pauseScene->render();
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

    ImGui::Text("Health");
    ImVec2 barSize(200.0f, 20.0f);
    ImGui::ProgressBar(healthPercent, barSize);
    ImGui::Spacing();

    if (gameManager) {
        const auto tod = gameManager->getTimeOfDay();
        ImGui::Text("Time: %02d:%02d:%02d", tod.hours, tod.minutes, tod.seconds);

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