#pragma once 

#include "player.h"
#include "game_manager.h"

namespace yc::gui {

class GameScene {

public:

    GameScene(Player* player, yc::GameManager* gameManager);

    void render();

private:

    Player* player;
    yc::GameManager* gameManager;
};

}