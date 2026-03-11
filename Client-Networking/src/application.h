#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "gl/common.h"

#include "camera.h"
#include "graphic/display.h"
#include "graphic/skybox.h"
#include "graphic/block_outline.h"
#include "world/world.h"
#include "graphic/crosshair.h"
#include "gui/gui.h"
#include "game_manager.h"
#include "settings.h"
#include "save_system.h"

namespace yc {

class Persistence;

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

    GameManager& getGameManager() { return gameManager; }
    const GameManager& getGameManager() const { return gameManager; }
    yc::world::World* getWorld() { return world; }
    const yc::world::World* getWorld() const { return world; }

    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }

    void applyCurrentSettings();

    void terminate();

    void pauseGame();
    void resumeGame();

    bool createNewSave(const std::string& name, int32_t seed);
    bool loadSave(const std::string& name);
    bool deleteSave(const std::string& name);
    std::vector<std::string> listSaves() const;
    bool connectToServer(const std::string& serverAddress, std::string& error);
    void disconnectFromServer();
    bool isServerConnected() const;
    bool shouldShowServerConnectInSaveSelect() const { return showServerConnectInSaveSelect; }

    void saveCurrentGame();

    bool isSaveSelectionActive() const { return saveSelectionActive; }
    void setSaveSelectionActive(bool value);

private:
    friend void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    friend void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    friend void sizeCallback(GLFWwindow* window, int width, int height);
    friend void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
    friend void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    static float deltaTime;

    bool paused;
    bool saveSelectionActive = true;

    Settings settings{};

    GameManager gameManager;

    graphic::Display display;
    gui::GUI gui;

    GLFWwindow* window;
    yc::world::World* world;
    Player* player;
    Persistence* persistence;

    SaveSystem saveSystem;
    bool showServerConnectInSaveSelect = true;
};

}