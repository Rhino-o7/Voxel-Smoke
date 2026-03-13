#include "graphic/display.h"
#include "resource.h"
#include "graphic/lighting.h"
#include "runtime_state.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace yc::graphic {

float quadVertices[] = {
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f
};

Display::Display() :
    lineMode(false)
{}

void Display::init() {
    // Initialize always-on scene helpers.
    crosshair.init();
    skybox.init();
    blockOutline.init();
    smokeRenderer.init();

    // Create framebuffers for a multi-pass pipeline:
    // 1) opaque scene, 2) transparent accumulation/reveal buffers.
    glGenFramebuffers(1, &opaqueFBO);
    glGenFramebuffers(1, &transparentFBO);

    // Opaque color target (final scene base before transparent composite).
    glGenTextures(1, &opaqueTexture);
    glBindTexture(GL_TEXTURE_2D, opaqueTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Shared depth target so transparent pass can depth-test against opaque geometry.
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Attach opaque color + depth targets.
    glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Opaque framebuffer is not complete!" << std::endl;

    // Weighted OIT accumulation target.
    glGenTextures(1, &accumTexture);
    glBindTexture(GL_TEXTURE_2D, accumTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Weighted OIT revealage target.
    glGenTextures(1, &revealTexture);
    glBindTexture(GL_TEXTURE_2D, revealTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Transparent framebuffer uses two color attachments + shared depth.
    glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, revealTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Transparent framebuffer is not complete!" << std::endl;

    const GLenum transparentDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, transparentDrawBuffers);

    // Full-screen quad used for composite and present passes.
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Display::prepareFrame() {
}

void Display::drawFrame(yc::Player* player, yc::world::World* world) {
    glm::vec4 zeroFillerVec(0.0f);
    glm::vec4 oneFillerVec(1.0f);

    // Resize/reallocate render targets to current viewport every frame.
    glBindTexture(GL_TEXTURE_2D, opaqueTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

    glBindTexture(GL_TEXTURE_2D, accumTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, revealTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, yc::runtime_state::GetViewportWidth(), yc::runtime_state::GetViewportHeight(), 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

    crosshair.update();

    // Pass 1: render opaque world into opaque framebuffer.
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float daylight = std::clamp(yc::graphic::lighting::ComputeDaylight(world->getSimTimeSec()), 0.0f, 1.0f);
    skybox.render(player->getCamera(), daylight);
    if (player->isSelectingBlock()) {
        blockOutline.render(player->getCamera(), player->getSelectingBlock());
    }

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    world->renderOpaque(player->getCamera());
    world->renderFlora(player->getCamera());

    smokeRenderer.render(player->getCamera(),
        world->getWindState(),
        world->getChimneyEmitters(),
        world->getSmokeSettings(),
        world->getSimTimeSec());

    // Pass 2: render transparent world using weighted OIT.
    // WebGL2 cannot use the blend funcs like glBlendFunci so transparent geometry is drawn twice:
    // - first into accum buffer
    // - then into reveal buffer
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);

    glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);

    // Accumulation buffer pass: additive weighted color/alpha.
    const GLenum accumOnlyDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
    glDrawBuffers(2, accumOnlyDrawBuffers);
    glBlendFunc(GL_ONE, GL_ONE);
    glClearBufferfv(GL_COLOR, 0, &zeroFillerVec[0]);
    world->renderTransparent(player->getCamera());

    // Revealage buffer pass: multiplicative transmittance term.
    const GLenum revealOnlyDrawBuffers[] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, revealOnlyDrawBuffers);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glClearBufferfv(GL_COLOR, 1, &oneFillerVec[0]);
    world->renderTransparent(player->getCamera());

    // Restore both attachments for later operations.
    const GLenum transparentDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, transparentDrawBuffers);

    // Pass 3: composite transparent buffers over opaque color target.
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);

    yc::Resource::CompositeShader.use();
    yc::Resource::CompositeShader.setInt("accum", 0);
    yc::Resource::CompositeShader.setInt("reveal", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accumTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, revealTexture);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Pass 4: present final color to backbuffer.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    yc::Resource::GrayScaleShader.use();
    yc::Resource::GrayScaleShader.setInt("screenTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, opaqueTexture);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    crosshair.render();
}

void Display::nextFrame() {
}

void Display::toggleLineMode() {
    lineMode = !lineMode;
}

void Display::setLineMode(bool enabled) {
    lineMode = enabled;
}

bool Display::isLineMode() const {
    return lineMode;
}

Display::~Display() {
}

}
