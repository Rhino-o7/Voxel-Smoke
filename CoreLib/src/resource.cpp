#include "resource.h"
#include <stb_image.h>

namespace yc {

yc::gl::Shader Resource::OpaqueShader;
yc::gl::Shader Resource::TransparentShader;
yc::gl::Shader Resource::FloraShader;
yc::gl::Shader Resource::GrayScaleShader;
yc::gl::Shader Resource::SkyBoxShader;
yc::gl::Shader Resource::BlockOutlineShader;
yc::gl::Shader Resource::CrossHairShader;
yc::gl::Shader Resource::CompositeShader;
yc::gl::Shader Resource::SmokeVolumeShader;

yc::gl::Texture Resource::GameTexure;
yc::gl::Texture Resource::CrossHairTexture;
yc::gl::Texture Resource::IconTexture;
yc::gl::Texture Resource::ResumeButtonTexture;
yc::gl::Texture Resource::HoveredResumeButtonTexture;
yc::gl::Texture Resource::BackButtonTexture;
yc::gl::Texture Resource::HoveredBackButtonTexture;

std::map<yc::world::BlockType, yc::gl::Texture> Resource::BlockIcons;

void Resource::Load() {

    Resource::OpaqueShader.loadFromFile("opaque.vert", "opaque.frag");
    Resource::TransparentShader.loadFromFile("transparent.vert", "transparent.frag");
    Resource::FloraShader.loadFromFile("flora.vert", "flora.frag");
    Resource::GrayScaleShader.loadFromFile("grayscale.vert", "grayscale.frag");
    Resource::SkyBoxShader.loadFromFile("skybox.vert", "skybox.frag");
    Resource::BlockOutlineShader.loadFromFile("block_outline.vert", "block_outline.frag");
    Resource::CrossHairShader.loadFromFile("crosshair.vert", "crosshair.frag");
    Resource::CompositeShader.loadFromFile("composite.vert", "composite.frag");
    Resource::SmokeVolumeShader.loadFromFile("smoke_volume.vert", "smoke_volume.frag");
    

    stbi_set_flip_vertically_on_load(true);
    Resource::GameTexure.loadFromFile("../CoreLib/resources/texture.png");
    Resource::CrossHairTexture.loadFromFile("../CoreLib/resources/crosshair.png");
    stbi_set_flip_vertically_on_load(false);

    Resource::ResumeButtonTexture.loadFromFile("../CoreLib/resources/gui/resume_button.png");
    Resource::HoveredResumeButtonTexture.loadFromFile("../CoreLib/resources/gui/hovered_resume_button.png");
    Resource::BackButtonTexture.loadFromFile("../CoreLib/resources/gui/back_button.png");
    Resource::HoveredBackButtonTexture.loadFromFile("../CoreLib/resources/gui/hovered_back_button.png");

    Resource::BlockIcons[world::BlockType::GRASS_BLOCK].loadFromFile("../CoreLib/resources/icons/grass_block.png");
    Resource::BlockIcons[world::BlockType::DIRT].loadFromFile("../CoreLib/resources/icons/dirt.png");
    Resource::BlockIcons[world::BlockType::GLASS].loadFromFile("../CoreLib/resources/icons/glass.png");
    Resource::BlockIcons[world::BlockType::STONE].loadFromFile("../CoreLib/resources/icons/stone.png");
    Resource::BlockIcons[world::BlockType::SNOW].loadFromFile("../CoreLib/resources/icons/snow.png");
    Resource::BlockIcons[world::BlockType::SAND].loadFromFile("../CoreLib/resources/icons/sand.png");
    Resource::BlockIcons[world::BlockType::WOOD].loadFromFile("../CoreLib/resources/icons/wood.png");
    Resource::BlockIcons[world::BlockType::LEAF].loadFromFile("../CoreLib/resources/icons/leaf.png");
    Resource::BlockIcons[world::BlockType::RED_FLOWER].loadFromFile("../CoreLib/resources/icons/red_flower.png");
    Resource::BlockIcons[world::BlockType::BLUE_FLOWER].loadFromFile("../CoreLib/resources/icons/blue_flower.png");
    Resource::BlockIcons[world::BlockType::YELLOW_FLOWER].loadFromFile("../CoreLib/resources/icons/yellow_flower.png");
    Resource::BlockIcons[world::BlockType::GRASS].loadFromFile("../CoreLib/resources/icons/grass.png");
    Resource::BlockIcons[world::BlockType::WATER].loadFromFile("../CoreLib/resources/icons/water.png");
    Resource::BlockIcons[world::BlockType::CROP].loadFromFile("../CoreLib/resources/icons/grass.png");

    //temp
	Resource::BlockIcons[world::BlockType::CHIMNEY].loadFromFile("../CoreLib/resources/icons/stone.png");
}

}