#pragma once

#include <stdint.h>
#include <string>
#include "gl/common.h"

#include "camera.h"
#include "graphic/display.h"
#include "graphic/skybox.h"
#include "graphic/block_outline.h"
#include "world/world.h"
#include "graphic/crosshair.h"
#include "gui/gui.h"
#include "game_clock.h"

namespace yc {

class Application {

public:
    static float GetDeltaTime();
    static int32_t Width;
    static int32_t Height;  
    
    Application(int32_t width, int32_t height, const std::string& title);
    ~Application();

    bool isStopped();

    void stop();

    void process();

    yc::graphic::Display* getDisplay();

    Player* getPlayer();

    void terminate();

    // CHANGED SEMANTICS: pause time, not the whole game simulation/render loop
    void pauseGame();
    void resumeGame();

    // NEW: optional API for UI/gameplay
    GameClock& getClock() { return clock; }
    const GameClock& getClock() const { return clock; }

private:
    friend void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    friend void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    friend void sizeCallback(GLFWwindow* window, int width, int height);
    friend void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
    friend void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    static float deltaTime;

    bool paused;

    GameClock clock;

    graphic::Display display;
    gui::GUI gui;

    GLFWwindow* window;
    yc::world::World* world;
    Player* player;
    Persistence* persistence;
};

}