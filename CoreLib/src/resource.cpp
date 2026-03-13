#include "resource.h"
#include <stb_image.h>
#include <filesystem>
#include <vector>

namespace yc {

namespace {
    std::string ResolveResourcePath(const std::string& relativePath) {
        namespace fs = std::filesystem;

        const std::vector<fs::path> resourceRoots = {
            fs::path("../CoreLib/resources"),
            fs::path("CoreLib/resources"),
            fs::path("resources"),
            fs::path("../resources"),
            fs::path("../../resources")
        };

        for (const auto& root : resourceRoots) {
            // Try multiple runtime layouts (IDE, packaged, or sibling project launches).
            const auto candidate = root / relativePath;
            std::error_code ec;
            if (fs::exists(candidate, ec)) {
                return candidate.string();
            }
        }

        return (fs::path("../CoreLib/resources") / relativePath).string();
    }
}

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

    // Compile/link all shader programs used by rendering passes.

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
    // 2D sprite/UI textures are authored top-left origin, so flip when needed.
    Resource::GameTexure.loadFromFile(ResolveResourcePath("texture.png"));
    Resource::CrossHairTexture.loadFromFile(ResolveResourcePath("crosshair.png"));
    stbi_set_flip_vertically_on_load(false);

    Resource::ResumeButtonTexture.loadFromFile(ResolveResourcePath("gui/resume_button.png"));
    Resource::HoveredResumeButtonTexture.loadFromFile(ResolveResourcePath("gui/hovered_resume_button.png"));
    Resource::BackButtonTexture.loadFromFile(ResolveResourcePath("gui/back_button.png"));
    Resource::HoveredBackButtonTexture.loadFromFile(ResolveResourcePath("gui/hovered_back_button.png"));

    Resource::BlockIcons[world::BlockType::GRASS_BLOCK].loadFromFile(ResolveResourcePath("icons/grass_block.png"));
    Resource::BlockIcons[world::BlockType::DIRT].loadFromFile(ResolveResourcePath("icons/dirt.png"));
    Resource::BlockIcons[world::BlockType::GLASS].loadFromFile(ResolveResourcePath("icons/glass.png"));
    Resource::BlockIcons[world::BlockType::STONE].loadFromFile(ResolveResourcePath("icons/stone.png"));
    Resource::BlockIcons[world::BlockType::SNOW].loadFromFile(ResolveResourcePath("icons/snow.png"));
    Resource::BlockIcons[world::BlockType::SAND].loadFromFile(ResolveResourcePath("icons/sand.png"));
    Resource::BlockIcons[world::BlockType::WOOD].loadFromFile(ResolveResourcePath("icons/wood.png"));
    Resource::BlockIcons[world::BlockType::LEAF].loadFromFile(ResolveResourcePath("icons/leaf.png"));
    Resource::BlockIcons[world::BlockType::RED_FLOWER].loadFromFile(ResolveResourcePath("icons/red_flower.png"));
    Resource::BlockIcons[world::BlockType::BLUE_FLOWER].loadFromFile(ResolveResourcePath("icons/blue_flower.png"));
    Resource::BlockIcons[world::BlockType::YELLOW_FLOWER].loadFromFile(ResolveResourcePath("icons/yellow_flower.png"));
    Resource::BlockIcons[world::BlockType::GRASS].loadFromFile(ResolveResourcePath("icons/grass.png"));
    Resource::BlockIcons[world::BlockType::WATER].loadFromFile(ResolveResourcePath("icons/water.png"));
    Resource::BlockIcons[world::BlockType::CROP].loadFromFile(ResolveResourcePath("icons/grass.png"));

    Resource::BlockIcons[world::BlockType::CHIMNEY].loadFromFile(ResolveResourcePath("chimney.png"));
}

}