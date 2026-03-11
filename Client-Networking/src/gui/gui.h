#pragma once

#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "gui/game_scene.h"
#include "gui/pause_scene.h"
#include "gui/save_select_scene.h"
#include "game_manager.h"

namespace yc::gui {

class GUI {

public:

    GUI();

    ~GUI();

    void init(GLFWwindow* window);

    void update(yc::Application* application, Player* player, yc::GameManager* gameManager);

    void render();

    void pause(yc::Application* application, PausePage page = PausePage::Main);

    void setPausePage(PausePage page);

    void resume();

private:

    void updateUiScale();

    std::shared_ptr<GameScene> gameScene;
    std::shared_ptr<PauseScene> pauseScene;
    std::shared_ptr<SaveSelectScene> saveSelectScene;
    yc::Application* application = nullptr;
    yc::GameManager* gameManager = nullptr;
    ImGuiStyle baseStyle{};
    float appliedUiScale = 1.0f;
};

}