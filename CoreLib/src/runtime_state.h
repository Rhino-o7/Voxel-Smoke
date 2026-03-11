#pragma once

#include <cstdint>

namespace yc::runtime_state {

void SetViewportSize(int32_t width, int32_t height);
int32_t GetViewportWidth();
int32_t GetViewportHeight();

void SetDeltaTime(float deltaTime);
float GetDeltaTime();

}
