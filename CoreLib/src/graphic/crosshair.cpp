#include "crosshair.h"
#include "runtime_state.h"

namespace yc::graphic {

void CrossHair::init() {
    this->mesh.init();
    this->mesh.bind();
    this->mesh.addIndices(Indices, 6);
    this->mesh.addStaticBuffer(2, &Vertices[0], 8);
    this->mesh.addStaticBuffer(2, &Texcoords[0], 8);
}

void CrossHair::update() {
    // Push current viewport and texture dimensions used by the crosshair shader.
    yc::Resource::CrossHairShader.use();
    yc::Resource::CrossHairShader.setInt("screen_width", yc::runtime_state::GetViewportWidth());
    yc::Resource::CrossHairShader.setInt("screen_height", yc::runtime_state::GetViewportHeight());
    yc::Resource::CrossHairShader.setInt("width", yc::Resource::CrossHairTexture.getWidth());
    yc::Resource::CrossHairShader.setInt("height", yc::Resource::CrossHairTexture.getHeight());
}

void CrossHair::render() {
    // Draw HUD crosshair with alpha blending on top of the 3D frame.
    Resource::CrossHairTexture.bind();
    Resource::CrossHairShader.use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    this->mesh.bind();
    this->mesh.draw();
    glDisable(GL_BLEND);
}

}