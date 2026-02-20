#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include "settings.h"
#include "game_manager.h"
#include "world/world.h"

namespace yc {

class SaveSystem {
public:
    struct SavePaths {
        std::filesystem::path saveRoot;
        std::filesystem::path regionsRoot;
        std::filesystem::path dataFile;
    };

    bool openOrCreate(const std::string& name);
    bool createNew(const std::string& name);

    std::vector<std::string> listSaves() const;
    bool deleteSave(const std::string& name);
    bool hasSaveData() const;

    bool saveGame(const Settings& settings, const GameManager& gameManager, const yc::world::World& world);
    bool loadGame(Settings& settings, GameManager& gameManager, yc::world::World& world);

    const SavePaths& getPaths() const { return paths; }
    const std::string& getCurrentSaveName() const { return currentSaveName; }

private:
    SavePaths paths{};
    std::string currentSaveName;
    std::filesystem::path baseFolder = "saves";
};

}