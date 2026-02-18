#pragma once

#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "gui/game_scene.h"
#include "gui/pause_scene.h"
#include "game_manager.h"

namespace yc::gui {

class GUI {

public:

    GUI();

    ~GUI();

    void init(GLFWwindow* window);

    void update(yc::Application* application, Player* player, yc::GameManager* gameManager);

    void render();

    void pause(yc::Application* application);

    void resume();

private:

    std::shared_ptr<GameScene> gameScene;
    std::shared_ptr<PauseScene> pauseScene;
    yc::GameManager* gameManager = nullptr;
};

}