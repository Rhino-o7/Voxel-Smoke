#pragma once

#include <string>
#include <vector>

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
};

}