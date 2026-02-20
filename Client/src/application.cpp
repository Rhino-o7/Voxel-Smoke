#include <iostream>
#include <algorithm>
#include <stb_image.h>

#include "application.h"
#include "resource.h"
#include "world/chunk.h"
#include "world/block.h"
#include "util/math.h"

namespace yc {

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    void sizeCallback(GLFWwindow* window, int width, int height);
    void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    namespace {
        constexpr int ChimneyHeightMin = 1;
        constexpr int ChimneyRadiusMin = 1;
        constexpr double ChimneyExitVelocityMin = 0.1;

        constexpr int ChimneyHeightStep = 1;
        constexpr int ChimneyRadiusStep = 1;
        constexpr double ChimneyExitVelocityStep = 0.5;

        GameManager::ChimneyParam NextChimneyParam(GameManager::ChimneyParam value) {
            switch (value) {
            case GameManager::ChimneyParam::Height:
                return GameManager::ChimneyParam::Radius;
            case GameManager::ChimneyParam::Radius:
                return GameManager::ChimneyParam::ExitVelocity;
            case GameManager::ChimneyParam::ExitVelocity:
            default:
                return GameManager::ChimneyParam::Height;
            }
        }

        void AdjustChimneySetting(GameManager& manager, int direction) {
            if (direction == 0) {
                return;
            }

            auto settings = manager.getChimneySettings();
            const auto param = manager.getActiveChimneyParam();

            switch (param) {
            case GameManager::ChimneyParam::Height:
                settings.height = std::max(ChimneyHeightMin, settings.height + direction * ChimneyHeightStep);
                break;
            case GameManager::ChimneyParam::Radius:
                settings.radius = std::max(ChimneyRadiusMin, settings.radius + direction * ChimneyRadiusStep);
                break;
            case GameManager::ChimneyParam::ExitVelocity:
                settings.exitVelocity = std::max(ChimneyExitVelocityMin, settings.exitVelocity + direction * ChimneyExitVelocityStep);
                break;
            }

            manager.setChimneySettings(settings);
        }

        bool IsChimneySelected(Application* app) {
            auto player = app ? app->getPlayer() : nullptr;
            return player && player->getCurrentBlockType() == yc::world::BlockType::CHIMNEY;
        }
    }

    Application::Application(int32_t width, int32_t height, const std::string& title) :
        paused(false) {

        Application::Width = width;
        Application::Height = height;

        // init GLFW
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // create window
        window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);

        if (window == NULL) {
            std::cout << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);

        // init opengl
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "Failed to initialize GLAD" << std::endl;
            exit(EXIT_FAILURE);
        }

        glfwSetWindowUserPointer(window, this);

        // callback
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallBack);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetFramebufferSizeCallback(window, sizeCallback);
        glfwSetScrollCallback(window, scrollCallback);

        // start in save selection mode
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        Resource::Load();

        saveSystem.openOrCreate("default");
        persistence = new Persistence(saveSystem.getPaths().regionsRoot.string());

        gui.init(window);

        world = new yc::world::World(persistence);
        world->init();

        player = new Player(50.0f, world);
        player->init();
        player->getCamera()->setFovDeg(settings.camera.fovDeg);

        gameManager.init(world, player);
        gameManager.applySettings(settings);
        gameManager.loadWindCsv(settings.game.windCsvPath);

        display.init();
        display.setSmokeSettings(settings.smoke);

        // set icon
        int iconWidth, iconHeight, iconChannel;

        uint8_t* iconData = stbi_load("./resources/yourcraft.png", &iconWidth, &iconHeight, &iconChannel, STBI_default);

        GLFWimage icons;
        icons.width = iconWidth;
        icons.height = iconHeight;
        icons.pixels = iconData;

        glfwSetWindowIcon(window, 1, &icons);
    }

    void Application::process() {

        // Real-time delta
        static double lastRealTime = glfwGetTime();
        const double now = glfwGetTime();
        const double realDt = now - lastRealTime;
        lastRealTime = now;

        // Keep existing render dt (used for FPS display / movement baseline)
        Application::deltaTime = static_cast<float>(realDt);

        if (!saveSelectionActive) {
            gameManager.update(realDt);
        }

        if (!paused && !saveSelectionActive) {
            if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
                world->reloadChunks();
            }
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                player->moveFront();
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                player->moveBack();
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                player->moveLeft();
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                player->moveRight();
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                player->moveUp();
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                player->moveDown();
            player->update();
            world->update(player->getCamera());
        }

        gui.update(this, player, &gameManager);

        display.drawFrame(player, world, gameManager.isSimulationRunning());

        gui.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    yc::graphic::Display* Application::getDisplay() {
        return &display;
    }

    bool Application::isStopped() {
        return glfwWindowShouldClose(window);
    }

    Player* Application::getPlayer() {
        return player;
    }

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        Application* app = (Application*)glfwGetWindowUserPointer(window);

        if (app->isSaveSelectionActive()) {
            return;
        }

        if (action == GLFW_PRESS && key == GLFW_KEY_T) {
            app->gameManager.toggleSimulation();
        }

        if (!app->paused) {
            if (glfwGetKey(app->window, GLFW_KEY_F3) == GLFW_PRESS) {
                app->display.toggleLineMode();
            }

            if (glfwGetKey(app->window, GLFW_KEY_F1) == GLFW_PRESS) {
                app->saveCurrentGame();
            }

            if (IsChimneySelected(app)) {
                if (action == GLFW_PRESS && key == GLFW_KEY_C) {
                    const auto next = NextChimneyParam(app->gameManager.getActiveChimneyParam());
                    app->gameManager.setActiveChimneyParam(next);
                }

                const bool increase = key == GLFW_KEY_RIGHT_BRACKET
                                   || key == GLFW_KEY_EQUAL
                                   || key == GLFW_KEY_KP_ADD;
                const bool decrease = key == GLFW_KEY_LEFT_BRACKET
                                   || key == GLFW_KEY_MINUS
                                   || key == GLFW_KEY_KP_SUBTRACT;

                if ((action == GLFW_PRESS || action == GLFW_REPEAT) && (increase || decrease)) {
                    AdjustChimneySetting(app->gameManager, increase ? 1 : -1);
                }
            }
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (app->paused) {
                app->resumeGame();
            }
            else {
                app->pauseGame();
            }
        }
    }

    void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        Application* app = (Application*)glfwGetWindowUserPointer(window);

        if (app->isSaveSelectionActive()) {
            return;
        }

        static float lastX = Application::Width / 2;
        static float lastY = Application::Height / 2;

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        if (!app->paused) {
            const float sensitivity = 0.2f;
            xoffset *= sensitivity;
            yoffset *= sensitivity;

            Camera* camera = app->player->getCamera();
            camera->setOrientation(
                camera->getPitch() + yoffset,
                camera->getYaw() + xoffset
            );
        }
    }
    // Block Place/Break
    void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods) {
        Application* app = (Application*)glfwGetWindowUserPointer(window);

        if (app->isSaveSelectionActive()) {
            return;
        }

        if (!app->paused) {
            if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
                if (app->player->isSelectingBlock()) {
                    if (app->player->getCurrentBlockType() == yc::world::BlockType::CHIMNEY) {
                        const auto& settings = app->gameManager.getChimneySettings();
                        app->world->spawnChimneyAt(
                            app->player->getSelectingBlock() + app->player->getSelectingFace(),
                            settings.height, settings.radius, settings.exitVelocity
                        );
                        return;
                    }
                    const auto placedPos = app->player->getSelectingBlock() + app->player->getSelectingFace();
                    app->world->setBlockDataIfLoadedAt(
                        placedPos,
                        { app->player->getCurrentBlockType() }
                    );

                    if (app->player->getCurrentBlockType() == yc::world::BlockType::CROP) {
                        app->gameManager.registerCropBlock(placedPos);
                    }
                }
            }

            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                if (app->player->isSelectingBlock()) {
                    const auto removedPos = app->player->getSelectingBlock();
                    app->world->destroyBlockIfLoaded(removedPos);
                    app->gameManager.unregisterCropBlock(removedPos);
                }
            }
        }
    }

    void sizeCallback(GLFWwindow* window, int width, int height) {
        Application* app = (Application*)glfwGetWindowUserPointer(window);
        glViewport(0, 0, width, height);
        Application::Width = width;
        Application::Height = height;
        app->player->getCamera()->update();
    }

    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        Application* app = (Application*)glfwGetWindowUserPointer(window);

        if (app->isSaveSelectionActive()) {
            return;
        }

        if (!app->paused) {
            const bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS
                || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

            if (IsChimneySelected(app) && altPressed) {
                const int direction = (yoffset > 0) ? 1 : -1;
                AdjustChimneySetting(app->gameManager, direction);
                return;
            }

            if (yoffset < 0) {
                app->player->nextSlot();
            }
            else {
                app->player->prevSlot();
            }
        }
    }

    void Application::stop() {
        glfwSetWindowShouldClose(window, true);
    }

    float Application::deltaTime = 1.0f / 60;

    float Application::GetDeltaTime() {
        return Application::deltaTime;
    }

    void Application::pauseGame() {
        if (saveSelectionActive) {
            return;
        }
        paused = true;
        gameManager.pauseTime();
        gui.pause(this);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void Application::resumeGame() {
        if (saveSelectionActive) {
            return;
        }
        paused = false;
        gameManager.resumeTime();
        gui.resume();
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void Application::terminate() {
        saveCurrentGame();
        glfwTerminate();
    }

    Application::~Application() {

    }

    int32_t Application::Width;
    int32_t Application::Height;

    bool Application::createNewSave(const std::string& name) {
        if (!saveSystem.createNew(name)) {
            return false;
        }

        world->clearChunks();
        world->clearChimneyEmitters();
        persistence->reset(saveSystem.getPaths().regionsRoot.string());

        gameManager.setSimTimeSec(0.0);
        gameManager.setSimulationRunning(false);

        return true;
    }

    bool Application::loadSave(const std::string& name) {
        if (!saveSystem.openOrCreate(name)) {
            return false;
        }

        world->clearChunks();
        world->clearChimneyEmitters();
        persistence->reset(saveSystem.getPaths().regionsRoot.string());

        if (saveSystem.hasSaveData()) {
            const bool loaded = saveSystem.loadGame(settings, gameManager, *world);
            if (!loaded) {
                return false;
            }
            gameManager.loadWindCsv(settings.game.windCsvPath);
        } else {
            gameManager.setSimTimeSec(0.0);
            gameManager.setSimulationRunning(false);
            gameManager.applySettings(settings);
        }

        if (player) {
            player->getCamera()->setFovDeg(settings.camera.fovDeg);
        }

        return true;
    }

    bool Application::deleteSave(const std::string& name) {
        return saveSystem.deleteSave(name);
    }

    std::vector<std::string> Application::listSaves() const {
        return saveSystem.listSaves();
    }

    void Application::saveCurrentGame() {
        world->saveChunks();
        saveSystem.saveGame(settings, gameManager, *world);
    }

    void Application::setSaveSelectionActive(bool value) {
        saveSelectionActive = value;
        paused = false;
        gui.resume();
        glfwSetInputMode(window, GLFW_CURSOR, value ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

}