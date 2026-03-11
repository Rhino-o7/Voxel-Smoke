#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace yc {
    class Application;
}

namespace yc::gui {

class SaveSelectScene {
public:
    explicit SaveSelectScene(Application* application);

    void render();

private:
    void refreshList();

    yc::Application* application = nullptr;
    std::vector<std::string> saveNames;
    int selectedIndex = -1;
    char newSaveName[64]{};
    char serverAddress[256] = "ws://127.0.0.1:9002";
    int32_t manualSeed = 0;
    int32_t randomSeed = 0;
    bool useRandomSeed = true;
    std::string connectError;
};

}