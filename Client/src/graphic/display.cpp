#include <glad/glad.h>
#include "graphic/display.h"
#include "application.h"
#include "resource.h"
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace yc::graphic {

float quadVertices[] = {
    // positions        // uv
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f
};

float cubeVertices[] = {
    // positions
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
     0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f,-0.5f,-0.5f,

    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
     0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f,

    -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f, -0.5f,-0.5f,-0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,

     0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
     0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,

    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
     0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,

    -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
     0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f
};

static bool ComputeSmokeBounds(
    const std::vector<yc::world::ChimneySource>& sources,
    const yc::world::WindState& wind,
    const yc::Settings::SmokeSettings& settings,
    glm::vec3& outMin,
    glm::vec3& outMax) {

    if (sources.empty() || wind.speed <= 1e-6) {
        return false;
    }

    const glm::dvec2 dir = wind.unitDirXZ();
    const glm::dvec2 perp(-dir.y, dir.x);

    const float maxDownwind = settings.boxDownwind;
    const float maxCrosswind = settings.boxCrosswind;
    const float maxVertical = settings.boxVertical;

    bool initialized = false;

    for (const auto& src : sources) {
        const glm::dvec3 base = src.worldPos + glm::dvec3(0.0, src.height, 0.0);
        const glm::dvec3 end = base + glm::dvec3(dir.x * maxDownwind, 0.0, dir.y * maxDownwind);
        const glm::dvec3 cross = glm::dvec3(perp.x * maxCrosswind, 0.0, perp.y * maxCrosswind);

        glm::dvec3 localMin = glm::min(base, end);
        glm::dvec3 localMax = glm::max(base, end);

        localMin -= glm::dvec3(std::abs(cross.x), maxVertical, std::abs(cross.z));
        localMax += glm::dvec3(std::abs(cross.x), maxVertical, std::abs(cross.z));

        if (!initialized) {
            outMin = glm::vec3(localMin);
            outMax = glm::vec3(localMax);
            initialized = true;
        } else {
            outMin = glm::min(outMin, glm::vec3(localMin));
            outMax = glm::max(outMax, glm::vec3(localMax));
        }
    }

    return initialized;
}

Display::Display():
    lineMode(false)
{}

void Display::init() {
    crosshair.init();
    skybox.init();
    blockOutline.init();

    glGenFramebuffers(1, &opaqueFBO);
    glGenFramebuffers(1, &transparentFBO);

    glGenTextures(1, &opaqueTexture);
	glBindTexture(GL_TEXTURE_2D, opaqueTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Application::Width, Application::Height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, Application::Width, Application::Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Opaque framebuffer is not complete!" << std::endl;

    glGenTextures(1, &accumTexture);
	glBindTexture(GL_TEXTURE_2D, accumTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Application::Width, Application::Height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &revealTexture);
	glBindTexture(GL_TEXTURE_2D, revealTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Application::Width, Application::Height, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, revealTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0); // opaque framebuffer's depth texture

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Transparent framebuffer is not complete!" << std::endl;

	const GLenum transparentDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, transparentDrawBuffers);

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

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void Display::setSmokeSettings(const yc::Settings::SmokeSettings& settings) {
    smokeSettings = settings;
}

void Display::renderSmokeVolume(yc::Camera* camera, yc::world::World* world) {
    const auto& sources = world->getChimneyEmitters();
    const auto& wind = world->getWindState();

    glm::vec3 boxMin{};
    glm::vec3 boxMax{};
    if (!ComputeSmokeBounds(sources, wind, smokeSettings, boxMin, boxMax)) {
        return;
    }

    constexpr int MaxSources = 32;
    const int count = std::min(static_cast<int>(sources.size()), MaxSources);

    std::vector<glm::vec4> posH(MaxSources, glm::vec4(0.0f));
    std::vector<glm::vec4> params(MaxSources, glm::vec4(0.0f));

    for (int i = 0; i < count; ++i) {
        const auto& src = sources[i];
        posH[i] = glm::vec4(static_cast<float>(src.worldPos.x),
                            static_cast<float>(src.worldPos.y),
                            static_cast<float>(src.worldPos.z),
                            static_cast<float>(src.height));
        params[i] = glm::vec4(static_cast<float>(src.exitVelocity),
                              static_cast<float>(src.radius),
                              0.0f,
                              0.0f);
    }

    const glm::vec3 center = 0.5f * (boxMin + boxMax);
    const glm::vec3 size = boxMax - boxMin;

    glm::mat4 model(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, size);

    const glm::vec3 camPos = camera->getPosition();
    const bool cameraInside =
        camPos.x >= boxMin.x && camPos.x <= boxMax.x &&
        camPos.y >= boxMin.y && camPos.y <= boxMax.y &&
        camPos.z >= boxMin.z && camPos.z <= boxMax.z;

    if (cameraInside) {
        glDepthFunc(GL_ALWAYS);
    }

    auto& shader = yc::Resource::SmokeVolumeShader;
    shader.use();
    shader.setMat4("projection_view", camera->getProjectionViewMatrix());
    shader.setMat4("model", model);
    shader.setVec3("uCameraPos", camPos);
    shader.setVec3("uBoxMin", boxMin);
    shader.setVec3("uBoxMax", boxMax);

    const glm::dvec2 dir = wind.unitDirXZ();
    shader.setFloat("uWindSpeed", static_cast<float>(wind.speed));
    shader.setVec2("uWindDirXZ", glm::vec2(static_cast<float>(dir.x), static_cast<float>(dir.y)));

    shader.setInt("uSourceCount", count);
    shader.setVec4Array("uSourcePosH", posH);
    shader.setVec4Array("uSourceParams", params);

    shader.setInt("uStepCount", std::max(1, smokeSettings.stepCount));
    shader.setFloat("uDensityScale", smokeSettings.densityScale);
    shader.setVec3("uSmokeColor", glm::vec3(smokeSettings.colorR, smokeSettings.colorG, smokeSettings.colorB));
    shader.setFloat("zNear", 0.1f);
    shader.setFloat("zFar", 5000.0f);

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    if (cameraInside) {
        glDepthFunc(GL_LESS);
    }
}

void Display::prepareFrame() {

    
}

void Display::drawFrame(yc::Player* player, yc::world::World* world) {
    glm::vec4 zeroFillerVec(0.0f);
	glm::vec4 oneFillerVec(1.0f);
    
	glBindTexture(GL_TEXTURE_2D, opaqueTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Application::Width, Application::Height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, Application::Width, Application::Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, accumTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Application::Width, Application::Height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, revealTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Application::Width, Application::Height, 0, GL_RED, GL_FLOAT, NULL);

    crosshair.update();

    if (lineMode) {
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    }

    // render into frame
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    skybox.render(player->getCamera());
    if (player->isSelectingBlock()) {
        blockOutline.render(player->getCamera(), player->getSelectingBlock());
    }

    glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

    // render opaque
    world->renderOpaque(player->getCamera());
    world->renderFlora(player->getCamera());

    // render transparent
    glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunci(0, GL_ONE, GL_ONE);
	glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

    glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
	glClearBufferfv(GL_COLOR, 0, &zeroFillerVec[0]);
	glClearBufferfv(GL_COLOR, 1, &oneFillerVec[0]);

    world->renderTransparent(player->getCamera());
    renderSmokeVolume(player->getCamera(), world);

    glDepthFunc(GL_ALWAYS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);

    yc::Resource::CompositeShader.use(); 

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, accumTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, revealTexture);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// draw to backbuffer (final pass)
	// -----

	// set render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE); // enable depth writes so glClear won't ignore clearing the depth buffer
	glDisable(GL_BLEND);

	// bind backbuffer
    if (lineMode) {
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// use screen shader
	yc::Resource::GrayScaleShader.use();

	// draw final screen quad
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

Display::~Display() {
    
}

}