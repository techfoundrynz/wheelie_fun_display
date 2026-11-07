#pragma once

namespace bike
{

constexpr double distance_in_units(double metres, bool metric) {
  return metres / (metric ? 1000.0 : 1609.344);
}

} // namespace bike
