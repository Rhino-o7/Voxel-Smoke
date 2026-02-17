#pragma once 

#include "player.h"
#include "world/world.h"

namespace yc::gui {

class GameScene {

public:

    GameScene(Player* player, yc::world::World* world);

    void render();

private:

    Player* player;
    yc::world::World* world;

};

}