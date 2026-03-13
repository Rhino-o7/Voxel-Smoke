#include <memory>

#include <emscripten/emscripten.h>

#include "application.h"

namespace {
    std::unique_ptr<yc::Application> g_app;

    void Tick() {
        if (!g_app || g_app->isStopped()) {
            if (g_app) {
                g_app->terminate();
                g_app.reset();
            }
            emscripten_cancel_main_loop();
            return;
        }

        g_app->process();
    }
}

int main() {
    g_app = std::make_unique<yc::Application>(1600, 800, "Voxel Web");
    g_app->getPlayer()->getCamera()->setOrientation(-89, 45);
    g_app->getPlayer()->getCamera()->setPosition({ 0, 70, 0 });

    emscripten_set_main_loop(Tick, 0, 1);
    return 0;
}
