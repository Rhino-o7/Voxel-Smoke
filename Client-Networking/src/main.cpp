// #pragma GCC optimize ("O3")
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl/shader.h"
#include "gl/texture.h"
#include "application.h"
#include <stb_image.h>
#include "world/chunk.h"
#include <vector>
#include "graphic/block_vertex.h"
#include <iostream>
#include "resource.h"
#include "world/world.h"
#include "world/world_generator.h"
#include "util/math.h"
#include "graphic/skybox.h"
#include "persistence.h"
#include "gl/framebuffer.h"
#include <filesystem>
#include <Windows.h>

namespace {
    void SetWorkingDirectoryToExecutableFolder() {
        char modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        if (length == 0) {
            return;
        }

        std::error_code ec;
        std::filesystem::current_path(std::filesystem::path(modulePath).parent_path(), ec);
    }
}




int main()
{
    SetWorkingDirectoryToExecutableFolder();

    yc::Application app(1600, 800, "Voxel");
 
    app.getPlayer()->getCamera()->setOrientation(-89, 45);
    app.getPlayer()->getCamera()->setPosition({ 0, 70, 0 });

    while (!app.isStopped()) {
        app.process();
    }

    app.terminate();

    return 0;
}