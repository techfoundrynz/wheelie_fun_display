#pragma once
#include "board.hpp"
#include "slint-platform.h"
#include <cstdint>

namespace slint_rendering
{

struct Work {
  uint32_t wait_us = 0, calls = 0, pixels = 0;
};

void initialize();

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation orientation);

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation orientation);

} // namespace slint_rendering
