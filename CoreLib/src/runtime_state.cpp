#include "runtime_state.h"

#include <algorithm>

namespace yc::runtime_state {

namespace {
int32_t g_viewportWidth = 1600;
int32_t g_viewportHeight = 900;
float g_deltaTime = 1.0f / 60.0f;
}

void SetViewportSize(int32_t width, int32_t height) {
    g_viewportWidth = std::max(1, width);
    g_viewportHeight = std::max(1, height);
}

int32_t GetViewportWidth() {
    return g_viewportWidth;
}

int32_t GetViewportHeight() {
    return g_viewportHeight;
}

void SetDeltaTime(float deltaTime) {
    g_deltaTime = std::max(0.0f, deltaTime);
}

float GetDeltaTime() {
    return g_deltaTime;
}

}
