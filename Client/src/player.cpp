#include "player.h"
#include "application.h"
#include "util/math.h"
#include "util/aabb.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace yc {

const float PlayerSize = 0.1;
constexpr float BaseGravity = 30.0f;
constexpr float WalkColliderRadius = 0.3f;
constexpr float WalkPlayerHeight = 2.0f;
constexpr float WalkEyeHeight = 1.62f;

const glm::vec3 PlayerBoundBox[8] = {
    { +PlayerSize, +PlayerSize, +PlayerSize },
    { +PlayerSize, +PlayerSize, -PlayerSize },
    { +PlayerSize, -PlayerSize, +PlayerSize },
    { -PlayerSize, +PlayerSize, +PlayerSize },
    { +PlayerSize, -PlayerSize, -PlayerSize },
    { -PlayerSize, +PlayerSize, -PlayerSize },
    { -PlayerSize, -PlayerSize, +PlayerSize },
    { -PlayerSize, -PlayerSize, -PlayerSize }
};

Player::Player(float speed, yc::world::World* world):
    speed(speed),
    world(world),
    selectingBlock(false),
    currentSlot(0) {

}

void Player::init() {
    camera.init();
    camera.update();

    inventory = {
        yc::world::BlockType::GRASS_BLOCK,
        yc::world::BlockType::DIRT,
        yc::world::BlockType::CROP,
		yc::world::BlockType::GLASS,
       
        yc::world::BlockType::CHIMNEY
    };
}

void Player::update() {
    if (!flyMode) {
        const float dt = Application::GetDeltaTime();
        const float gravityAccel = BaseGravity * gravityMultiplier;
        verticalVelocity -= gravityAccel * dt;

        const float deltaY = verticalVelocity * dt;
        if (std::fabs(deltaY) > 0.0f) {
            auto position = camera.getPosition();
            const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(deltaY) / 0.05f)));
            const float stepY = deltaY / static_cast<float>(steps);

            for (int i = 0; i < steps; ++i) {
                const auto next = position + glm::vec3(0.0f, stepY, 0.0f);
                if (collidesAtPosition(next)) {
                    if (stepY < 0.0f) {
                        onGround = true;
                    }
                    verticalVelocity = 0.0f;
                    break;
                }

                onGround = false;
                position = next;
            }

            camera.setPosition(position);
        }

        if (!onGround) {
            const auto below = camera.getPosition() + glm::vec3(0.0f, -0.06f, 0.0f);
            if (collidesAtPosition(below)) {
                onGround = true;
                verticalVelocity = 0.0f;
            }
        }
    }

    auto coord = camera.getPosition();
    auto direction = camera.getDirection();
    auto raycast = world->raycastCheck(coord, direction, false, true);
    
    selectingBlock = raycast.block.getType() != yc::world::BlockType::NONE;

    if (selectingBlock) {
        selectingBlockCoord = raycast.coord;
        selectingFace = raycast.face;
        selectingBlockType = raycast.block.getType();
    }

}

Camera* Player::getCamera() {
    return &camera;
}

glm::ivec3 Player::getSelectingBlock() {
    return selectingBlockCoord;
}

glm::ivec3 Player::getSelectingFace() {
    return selectingFace;
}

yc::world::BlockType Player::getSelectingBlockType() {
    return selectingBlockType;
}

bool Player::isSelectingBlock() {
    return selectingBlock;
}

void Player::moveFront() {
    auto delta = speed * Application::GetDeltaTime() * camera.getFront();
    if (!flyMode) {
        delta.y = 0.0f;
        moveWithCollision(delta);
        return;
    }
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

void Player::moveBack() {
    auto delta = - speed * Application::GetDeltaTime() * camera.getFront();
    if (!flyMode) {
        delta.y = 0.0f;
        moveWithCollision(delta);
        return;
    }
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

void Player::moveRight() {
    auto delta = speed * Application::GetDeltaTime() * camera.getRight();
    if (!flyMode) {
        delta.y = 0.0f;
        moveWithCollision(delta);
        return;
    }
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

void Player::moveLeft() {
    auto delta = - speed * Application::GetDeltaTime() * camera.getRight();
    if (!flyMode) {
        delta.y = 0.0f;
        moveWithCollision(delta);
        return;
    }
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

bool Player::checkIntersect(const glm::vec3& delta) {
    if (!flyMode) {
        return collidesAtPosition(camera.getPosition() + delta);
    }

    auto currentCoord = camera.getPosition() + delta;
    auto direction = glm::normalize(delta);

    for (int i=0;i<8;++i) {
        auto raycast = world->raycastCheck(
            currentCoord + PlayerBoundBox[i],
            direction,
            true,
            true
        );

        if (raycast.block.getType() == world::BlockType::NONE) {
            continue;
        }

        bool check = util::IsInRect(
            currentCoord + PlayerBoundBox[i],
            raycast.coord + glm::ivec3(1, 1, 1),
            raycast.coord
        );

        if (check) return true;
    }

    return false;
}

bool Player::isSolidBlock(world::BlockType type) const {
    switch (type) {
    case world::BlockType::AIR:
    case world::BlockType::NONE:
    case world::BlockType::WATER:
    case world::BlockType::RED_FLOWER:
    case world::BlockType::YELLOW_FLOWER:
    case world::BlockType::BLUE_FLOWER:
        return false;
    default:
        return true;
    }
}

bool Player::collidesAtPosition(const glm::vec3& position) const {
    const glm::vec3 minPos(
        position.x - WalkColliderRadius,
        position.y - WalkEyeHeight,
        position.z - WalkColliderRadius);
    const glm::vec3 maxPos(
        position.x + WalkColliderRadius,
        position.y - WalkEyeHeight + WalkPlayerHeight,
        position.z + WalkColliderRadius);

    const int minX = static_cast<int>(std::floor(minPos.x));
    const int maxX = static_cast<int>(std::floor(maxPos.x));
    const int minY = static_cast<int>(std::floor(minPos.y));
    const int maxY = static_cast<int>(std::floor(maxPos.y));
    const int minZ = static_cast<int>(std::floor(minPos.z));
    const int maxZ = static_cast<int>(std::floor(maxPos.z));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                const auto type = world->getBlockDataIfLoadedAt({ x, y, z }).getType();
                if (isSolidBlock(type)) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Player::moveWithCollision(const glm::vec3& delta) {
    if (glm::dot(delta, delta) <= 0.0f) {
        return;
    }

    auto position = camera.getPosition();

    const auto tryAxisMove = [&](float axisDelta, const glm::vec3& axis) {
        if (std::fabs(axisDelta) <= 0.0f) {
            return;
        }

        const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(axisDelta) / 0.05f)));
        const float step = axisDelta / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            const glm::vec3 next = position + axis * step;
            if (collidesAtPosition(next)) {
                break;
            }
            position = next;
        }
    };

    tryAxisMove(delta.x, glm::vec3(1.0f, 0.0f, 0.0f));
    tryAxisMove(delta.y, glm::vec3(0.0f, 1.0f, 0.0f));
    tryAxisMove(delta.z, glm::vec3(0.0f, 0.0f, 1.0f));

    camera.setPosition(position);
}

void Player::nextSlot() {
    currentSlot = (currentSlot+1) % inventory.size(); 
}

void Player::prevSlot() {
    currentSlot = (currentSlot+inventory.size()-1) % inventory.size();
}

uint32_t Player::getCurrentSlot() {
    return currentSlot;
}

world::BlockType Player::getCurrentBlockType() {
    return inventory[currentSlot];
}

std::vector<yc::world::BlockType> Player::getInventory() {
    return inventory;
}

void Player::moveUp() {
    if (!flyMode) {
        return;
    }
    auto delta = speed * Application::GetDeltaTime() * VectorUp;
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

void Player::moveDown() {
    if (!flyMode) {
        return;
    }
    auto delta = - speed * Application::GetDeltaTime() * VectorUp;
    if (checkIntersect(delta)) return;
    camera.setPosition(camera.getPosition() + delta);
}

void Player::jump() {
    if (flyMode || !onGround) {
        return;
    }

    const float gravityAccel = BaseGravity * gravityMultiplier;
    verticalVelocity = std::sqrt(std::max(0.1f, 2.0f * gravityAccel * jumpHeight));
    onGround = false;
}

bool Player::isFlyMode() const {
    return flyMode;
}

void Player::setFlyMode(bool enabled) {
    flyMode = enabled;
    verticalVelocity = 0.0f;
    if (flyMode) {
        onGround = false;
    }
}

void Player::toggleFlyMode() {
    setFlyMode(!flyMode);
}

void Player::setMoveSpeed(float value) {
    speed = std::max(0.1f, value);
}

void Player::setGravityMultiplier(float value) {
    gravityMultiplier = std::clamp(value, 0.1f, 10.0f);
}

void Player::setJumpHeight(float value) {
    jumpHeight = std::max(0.25f, value);
}

}