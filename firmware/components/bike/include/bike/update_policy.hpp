#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace bike
{

inline bool trusted_update_url(std::string_view url) {
  if (!url.starts_with("https://") || url.size() > 4096)
    return false;
  const auto end = url.find_first_of("/?#", 8);
  const auto host = url.substr(8, end == std::string_view::npos ? end : end - 8);
  return host == "api.github.com" || host == "github.com" || host == "release-assets.githubusercontent.com" ||
         host == "objects.githubusercontent.com";
}

inline std::optional<std::array<unsigned char, 32>> release_digest(std::string_view text) {
  if (!text.starts_with("sha256:") || text.size() != 71)
    return std::nullopt;
  std::array<unsigned char, 32> result{};
  auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9')
      return value - '0';
    if (value >= 'a' && value <= 'f')
      return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
      return value - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < result.size(); ++i) {
    const int hi = nibble(text[7 + 2 * i]), lo = nibble(text[8 + 2 * i]);
    if (hi < 0 || lo < 0)
      return std::nullopt;
    result[i] = static_cast<unsigned char>((hi << 4) | lo);
  }
  return result;
}

} // namespace bike
