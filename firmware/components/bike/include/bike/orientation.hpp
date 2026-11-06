#pragma once
#include <algorithm>

namespace bike
{

#ifndef HOR_RES
  #define HOR_RES 410 // portable host-test default
#endif
#ifndef VER_RES
  #define VER_RES 502
#endif
constexpr int panel_width = HOR_RES, panel_height = VER_RES;

static_assert(panel_width > 0 && panel_height > 0 && panel_width % 2 == 0 && panel_height % 2 == 0,
              "The AMOLED renderer requires positive, even native dimensions");

enum class Orientation {
  Portrait = 0,
  Landscape = 90,
  PortraitInverted = 180,
  LandscapeInverted = 270
};

struct Point {
  int x, y;
};

constexpr bool valid_rotation(int degrees) {
  return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
}

constexpr Orientation orientation_from_degrees(int degrees) {
  return valid_rotation(degrees) ? static_cast<Orientation>(degrees) : Orientation::Portrait;
}

constexpr bool is_landscape(Orientation o) {
  return o == Orientation::Landscape || o == Orientation::LandscapeInverted;
}

constexpr int display_width(Orientation o, int w = panel_width, int h = panel_height) {
  return is_landscape(o) ? h : w;
}

constexpr int display_height(Orientation o, int w = panel_width, int h = panel_height) {
  return is_landscape(o) ? w : h;
}

// Keep native touch coordinates until this inverse of Slint's rendering rotation.
constexpr Point to_logical(Point raw, Orientation o, int w = panel_width, int h = panel_height) {
  raw.x = std::clamp(raw.x, 0, w - 1);
  raw.y = std::clamp(raw.y, 0, h - 1);
  switch (o) {
  case Orientation::Landscape:
    return {raw.y, w - 1 - raw.x};
  case Orientation::PortraitInverted:
    return {w - 1 - raw.x, h - 1 - raw.y};
  case Orientation::LandscapeInverted:
    return {h - 1 - raw.y, raw.x};
  default:
    return raw;
  }
}

constexpr Point to_panel(Point logical, Orientation o, int w = panel_width, int h = panel_height) {
  switch (o) {
  case Orientation::Landscape:
    return {w - 1 - logical.y, logical.x};
  case Orientation::PortraitInverted:
    return {w - 1 - logical.x, h - 1 - logical.y};
  case Orientation::LandscapeInverted:
    return {logical.y, h - 1 - logical.x};
  default:
    return logical;
  }
}

} // namespace bike
