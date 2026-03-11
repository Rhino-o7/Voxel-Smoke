#pragma once

#include "camera.h"
#include "world/world.h"

namespace yc {

class Player {

public:

    Player(float speed, yc::world::World* world);

    Camera* getCamera();

    glm::ivec3 getSelectingBlock();
    glm::ivec3 getSelectingFace();
    yc::world::BlockType getSelectingBlockType();

    void moveFront();
    void moveBack();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void jump();

    bool isFlyMode() const;
    void setFlyMode(bool enabled);
    void toggleFlyMode();

    void setMoveSpeed(float value);
    void setGravityMultiplier(float value);
    void setJumpHeight(float value);

    bool checkIntersect(const glm::vec3& delta);

    void init();

    bool isSelectingBlock();

    void update();

    void nextSlot();

    void prevSlot();

    uint32_t getCurrentSlot();

    world::BlockType getCurrentBlockType();

    std::vector<yc::world::BlockType> getInventory();

private:
    bool isSolidBlock(world::BlockType type) const;
    bool collidesAtPosition(const glm::vec3& position) const;
    void moveWithCollision(const glm::vec3& delta);

    glm::ivec3 selectingBlockCoord;
    glm::ivec3 selectingFace;
    yc::world::BlockType selectingBlockType;

    bool selectingBlock;
    float speed;
    bool flyMode = true;
    float gravityMultiplier = 1.0f;
    float jumpHeight = 1.0f;
    float verticalVelocity = 0.0f;
    bool onGround = false;

    Camera camera;
    yc::world::World* world;
    std::vector<yc::world::BlockType> inventory;
    uint32_t currentSlot;
};

}