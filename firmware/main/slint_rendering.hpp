#pragma once
#include "bike/page_slide.hpp"
#include "board.hpp"
#include "slint-platform.h"
#include <cstdint>
#include <functional>

namespace slint_rendering
{

struct Work {
  uint32_t wait_us = 0, calls = 0, pixels = 0;
};

struct PageSlideState {
  bool active = false;
  bool content_active = false, available = false;
  int incoming = -1, outgoing = -1, direction = 1;
  float progress = 0;
  bike::SlideRect viewport;
};

void configure_page_slides(std::function<PageSlideState()> read, std::function<void(int)> capture);

bool page_slide_active();

void prepare_pointer_input();

void initialize();

slint::platform::SoftwareRenderer::RenderingRotation rotation(bike::Orientation orientation);

Work render(slint::platform::SoftwareRenderer &renderer, board::Display &display, bike::Orientation orientation);

} // namespace slint_rendering
