#pragma once

#include <glm/glm.hpp>
#include "gl/framebuffer.h"
#include "graphic/crosshair.h"
#include "graphic/skybox.h"
#include "graphic/block_outline.h"
#include "player.h"
#include "world/world.h"
#include "settings.h"

namespace yc::graphic {

class Display {

public:
    Display();

    ~Display();

    void init();

    void prepareFrame();

    void drawFrame(yc::Player* player, yc::world::World* world, bool renderSmoke);

    void nextFrame();

    void toggleLineMode();

    void setSmokeSettings(const yc::Settings::SmokeSettings& settings);

private:
    void renderSmokeVolume(yc::Camera* camera, yc::world::World* world, bool useWeightedOIT);

    glm::vec3 clearColor = { 0.2f, 0.3f, 0.3f };

    bool lineMode;

    unsigned int opaqueFBO, transparentFBO;
    unsigned int opaqueTexture;
    unsigned int depthTexture;
    unsigned int accumTexture;
    unsigned int revealTexture;
    unsigned int quadVAO, quadVBO;
    unsigned int cubeVAO, cubeVBO;

    yc::Settings::SmokeSettings smokeSettings{};

    SkyBox skybox;
    BlockOutline blockOutline;
    CrossHair crosshair;
};

}