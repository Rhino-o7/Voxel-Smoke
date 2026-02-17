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

void GUI::update(yc::Application* application, yc::world::World* world, Player* player) {
    if (gameScene == nullptr) {
        gameScene = std::make_shared<GameScene>(player, world);
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

    // TODO: replace these placeholder values with real game data later
    float healthPercent = 0.75f; // 0.0f - 1.0f
    float timeSeconds   = 123.4f;
    float windSpeedMs   = 5.6f;

    // Healthbar
    ImGui::Text("Health");
    ImVec2 barSize(200.0f, 20.0f);
    ImGui::ProgressBar(healthPercent, barSize);
    ImGui::Spacing();

    // Timer (MM:SS)
    int totalSeconds = static_cast<int>(timeSeconds);
    int minutes      = totalSeconds / 60;
    int seconds      = totalSeconds % 60;
    ImGui::Text("Time: %02d:%02d", minutes, seconds);

    // Windspeed
    ImGui::Text("Wind: %.1f m/s", windSpeedMs);

    ImGui::End();
    // --- end HUD ---

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}