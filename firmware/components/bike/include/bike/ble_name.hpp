#pragma once
#include <string_view>

namespace bike
{

// The app discovers bikes by a name containing WFB, independent of NUS UUIDs.
constexpr bool wfb_name_matches(std::string_view name) {
  for (size_t i = 0; i + 2 < name.size(); ++i) {
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; };
    if (lower(name[i]) == 'w' && lower(name[i + 1]) == 'f' && lower(name[i + 2]) == 'b')
      return true;
  }
  return false;
}

constexpr bool wheel_e_name_matches(std::string_view name) {
  for (size_t i = 0; i + 6 < name.size(); ++i) {
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; };
    constexpr std::string_view expected = "wheel-e";
    bool match = true;
    for (size_t j = 0; j < expected.size(); ++j)
      match &= lower(name[i + j]) == expected[j];
    if (match)
      return true;
  }
  return false;
}

constexpr bool preferred_bike_name(std::string_view name) {
  return wfb_name_matches(name) || wheel_e_name_matches(name);
}

} // namespace bike
