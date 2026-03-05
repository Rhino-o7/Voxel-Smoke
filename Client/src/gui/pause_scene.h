#pragma once

#include <array>
#include <string>

namespace yc {
    class Application;
}

namespace yc::gui {

enum class PausePage {
    Main = 0,
    ChimneyManager,
    Settings
};

class PauseScene {

public:

    PauseScene(Application* application);
    void setPage(PausePage page);

    void render();

private:
    void renderMainPage();
    void renderChimneyPage();
    void renderSettingsPage();
    void syncWindPathBuffer();

    Application* application;
    PausePage activePage = PausePage::Main;
    std::array<char, 512> windCsvPathBuffer{};
    bool windCsvPathBufferInitialized = false;
    std::string windCsvPathError;

};

}